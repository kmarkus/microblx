/*
 * lfbm.h - multi-producer mode of the lfb broadcast ring
 *
 * Copyright (C) 2026 Markus Klotzbuecher <mk@mkio.de>
 *
 * SPDX-License-Identifier: MPL-2.0
 */

/*
 * lfbm lets any number of producers, in any number of processes,
 * write into one lossy broadcast ring without a lock: no producer
 * ever waits for another one. It reuses the lfb header and the
 * lfb_shm lifecycle (see lfbm_shm.h), but has its own write and read
 * paths. lfb_attach refuses such a ring, as lfbm_attach does a plain
 * one.
 *
 * Each slot starts with a 64 bit sequence word, so lfb's frame_size
 * is LFBM_SLOT_SZ(payload). lfb_t.w counts tickets (it is not lfb's
 * wrap|offset word), and ticket t owns slot t & (depth - 1). The depth
 * must be a power of two, to avoid a division in the write path, which
 * is a libgcc call on 32 bit targets:
 *
 *   seq == 0            slot was never written
 *   seq == 2 * (t + 1)      ticket t committed
 *   seq == 2 * (t + 1) + 1  ticket t being written
 *
 * A writer takes a ticket with fetch_add and claims the slot with a
 * CAS on seq. If the slot is held by a stalled writer, or a newer
 * ticket got there first, it abandons the ticket and takes the next
 * one. A claimed slot has a single owner, so writers cannot corrupt
 * each other, and one preempted anywhere only holds on to its slot.
 *
 * A writer which was lapped while holding its slot commits under the
 * newest ticket of that slot: those tickets were abandoned because of
 * it, and readers would otherwise wait for them in vain. A ticket
 * taken right between reading w and publishing is missed: readers
 * then wait for it up to the stall limit and count a frame as skipped
 * which was not lost. It can't be stamped later on, as a reader may
 * already have the frame and would get it twice. Closing this takes
 * a slot state for "abandoned", if it ever shows up.
 *
 * Readers validate seq around the copy (seqlock-style) and deliver
 * in ticket order, i.e. per-producer order. Across producers the
 * order is that of the tickets, so timestamps taken before the
 * ticket can be slightly out of order.
 *
 * Differences to single-producer lfb:
 *
 *  - lfbm_read may stop at a slot whose writer has not committed yet
 *    and returns 0 until it does. If that takes longer than
 *    rd->stall_limit_ns, the frame is given up on (rd->skipped); a
 *    writer that resumes after this delivers to nobody at that cursor.
 *
 *  - a producer killed mid-write leaves its slot marked as being
 *    written for good: the ring is one slot shorter from then on.
 *    Readers and writers step over such a slot. If it is the one
 *    being waited for, a reader skips it after the stall limit once.
 *
 *  - lfbm_write can fail with -EBUSY after LFBM_MAX_TRIES occupied
 *    slots, which takes that many producers stalled at once.
 *
 *  - lfbm_read needs clock_gettime, i.e. POSIX (and unlike lfb.h, it
 *    doesn't build with strict -std=c11, without _POSIX_C_SOURCE).
 *
 *  - needs 64 bit atomics: seq and w are assumed not to wrap.
 *    skipped: LFB_CTR_BITS 32, add when a target needs it.
 */

#ifndef _LFBM_H_
#define _LFBM_H_

#include "lfb.h"

#include <time.h>

_Static_assert(LFB_CTR_BITS == 64, "lfbm requires 64 bit lfb words");

#ifndef LFBM_MAX_TRIES
#define LFBM_MAX_TRIES 16
#endif

/* default for how long a reader waits for an uncommitted slot */
#ifndef LFBM_STALL_NS
#define LFBM_STALL_NS 1000000000ULL
#endif

#define LFBM_SEQ_SZ	((uint32_t)sizeof(uint64_t))

/* lfb frame_size for a payload, which must be a multiple of 8 */
#define LFBM_SLOT_SZ(payload_sz) ((payload_sz) + LFBM_SEQ_SZ)

#define LFBM_DONE(t)	(((uint64_t)(t) + 1) * 2)
#define LFBM_BUSY(t)	(LFBM_DONE(t) + 1)

/* per-consumer read state, private to one reader thread */
typedef struct lfbm_rd {
	const lfb_t *b;
	uint64_t r;		/* next ticket to read */
	uint64_t stall_ns;	/* when r was first found uncommitted */
	uint64_t stall_limit_ns; /* LFBM_STALL_NS, adjust after lfbm_rd_init */
	uint64_t gave_up;	/* last ticket given up on + 1, 0: none */
	unsigned long overruns;	/* nr of overruns since lfbm_rd_init */
	unsigned long skipped;	/* frames given up on since lfbm_rd_init */
} lfbm_rd_t;

/* a slot's seq sits at its start, so payloads must keep it aligned */
static inline int lfbm_geometry_ok(uint32_t payload_sz, lfb_word_t depth)
{
	return payload_sz > 0 && payload_sz % LFBM_SEQ_SZ == 0 &&
		payload_sz <= UINT32_MAX - LFBM_SEQ_SZ &&
		depth != 0 && (depth & (depth - 1)) == 0;
}

/**
 * @brief size in bytes of a ring, see lfb_memsz
 *
 * @param payload_sz frame size, a multiple of 8
 * @param depth number of frames, a power of two
 * @param user_sz size of the caller-owned user area
 * @return required size, 0 if the parameters are invalid
 */
static inline size_t lfbm_memsz(uint32_t payload_sz, lfb_word_t depth,
				uint32_t user_sz)
{
	if (!lfbm_geometry_ok(payload_sz, depth))
		return 0;

	return lfb_memsz(LFBM_SLOT_SZ(payload_sz), depth, user_sz);
}

/**
 * @brief prepare a ring without publishing it, see lfb_prepare
 *
 * @param mem memory region
 * @param memsz size of the region, >= lfbm_memsz()
 * @param payload_sz frame size, a multiple of 8
 * @param depth number of frames, a power of two
 * @param user_sz size of the caller-owned user area
 * @return the prepared buffer, or NULL on invalid parameters
 */
static inline lfb_t *lfbm_prepare(void *mem, size_t memsz, uint32_t payload_sz,
				  lfb_word_t depth, uint32_t user_sz)
{
	lfb_t *b;

	if (!lfbm_geometry_ok(payload_sz, depth))
		return NULL;

	b = lfb_prepare_mode(mem, memsz, LFBM_SLOT_SZ(payload_sz), depth,
			     user_sz, LFB_MODE_MP);

	/* seq must be 0 in unused slots */
	if (b != NULL)
		memset(lfb_data(b), 0, (size_t)depth * b->frame_size);

	return b;
}

/**
 * @brief initialize a ring, see lfb_init
 *
 * @return the initialized buffer, or NULL as per lfbm_prepare
 */
static inline lfb_t *lfbm_init(void *mem, size_t memsz, uint32_t payload_sz,
			       lfb_word_t depth, uint32_t user_sz)
{
	lfb_t *b = lfbm_prepare(mem, memsz, payload_sz, depth, user_sz);

	if (b != NULL)
		lfb_publish(b);

	return b;
}

/**
 * @brief attach to an initialized ring, see lfb_attach
 *
 * @return the buffer, or NULL on any mismatch
 */
static inline lfb_t *lfbm_attach(void *mem, size_t memsz)
{
	return lfb_attach_mode(mem, memsz, LFB_MODE_MP);
}

static inline _Atomic uint64_t *lfbm_seq(lfb_t *b, uint64_t t)
{
	return (_Atomic uint64_t *)(void *)
		(lfb_data(b) + (size_t)(t & (b->depth - 1)) * b->frame_size);
}

static inline const _Atomic uint64_t *lfbm_cseq(const lfb_t *b, uint64_t t)
{
	return (const _Atomic uint64_t *)(const void *)
		(lfb_cdata(b) + (size_t)(t & (b->depth - 1)) * b->frame_size);
}

/* newest ticket issued so far that maps to the slot of t */
static inline uint64_t lfbm_newest(const lfb_t *b, uint64_t w, uint64_t t)
{
	return t + ((w - 1 - t) & ~(uint64_t)(b->depth - 1));
}

/* publish a claimed slot, see the top about lapped writers */
static inline void lfbm_commit(lfb_t *b, _Atomic uint64_t *seq, uint64_t t)
{
	t = lfbm_newest(b, atomic_load_explicit(&b->w, memory_order_relaxed), t);
	atomic_store_explicit(seq, LFBM_DONE(t), memory_order_release);
}

/**
 * @brief write a frame
 *
 * Safe to call concurrently from any thread or process, and doesn't
 * block: the oldest frame is overwritten if the buffer is full.
 *
 * @param b the buffer
 * @param frame payload to write (frame_size - LFBM_SEQ_SZ bytes)
 * @return 0, or -EBUSY if too many slots were occupied and the frame
 *         was dropped
 */
static inline int lfbm_write(lfb_t *b, const void *frame)
{
	for (int i = 0; i < LFBM_MAX_TRIES; i++) {
		uint64_t t = atomic_fetch_add_explicit(&b->w, 1,
						       memory_order_relaxed);
		_Atomic uint64_t *seq = lfbm_seq(b, t);
		uint64_t e = atomic_load_explicit(seq, memory_order_relaxed);

		/* occupied, or we were lapped */
		if ((e & 1) != 0 || e >= LFBM_DONE(t))
			continue;

		if (!atomic_compare_exchange_strong_explicit(
			    seq, &e, LFBM_BUSY(t), memory_order_acquire,
			    memory_order_relaxed))
			continue;

		/* readers must not see payload before the busy mark */
		atomic_thread_fence(memory_order_release);

		lfb_frame_copy((uint8_t *)seq + LFBM_SEQ_SZ, frame,
			       b->frame_size - LFBM_SEQ_SZ);

		lfbm_commit(b, seq, t);
		return 0;
	}

	return -EBUSY;
}

/* ticket of the oldest frame which survives a catching up reader */
static inline uint64_t lfbm_oldest(const lfb_t *b, uint64_t w)
{
	uint64_t keep = b->depth - LFB_CRUSH(b->depth);

	return (w > keep) ? w - keep : 0;
}

/**
 * @brief reposition a reader at the oldest or newest frame
 *
 * Its counters and stall limit stay as they are.
 *
 * @param b an attached buffer
 * @param rd a reader set up by lfbm_rd_init
 * @param whence LFB_OLDEST or LFB_NEWEST
 */
static inline void lfbm_seek(const lfb_t *b, lfbm_rd_t *rd, int whence)
{
	uint64_t w = atomic_load_explicit(&b->w, memory_order_acquire);

	rd->b = b;
	rd->r = (whence == LFB_NEWEST) ? w : lfbm_oldest(b, w);
	rd->stall_ns = 0;
}

/**
 * @brief set up a reader
 *
 * @param b an attached buffer
 * @param rd reader state to initialize
 * @param whence where to start, LFB_OLDEST or LFB_NEWEST
 */
static inline void lfbm_rd_init(const lfb_t *b, lfbm_rd_t *rd, int whence)
{
	rd->stall_limit_ns = LFBM_STALL_NS;
	rd->gave_up = 0;
	rd->overruns = 0;
	rd->skipped = 0;
	lfbm_seek(b, rd, whence);
}

/* resync after losing frames, as per LFB_OLDEST */
static inline int lfbm_overrun(lfbm_rd_t *rd)
{
	const lfb_t *b = rd->b;
	uint64_t w = atomic_load_explicit(&b->w, memory_order_acquire);

	rd->overruns++;
	rd->stall_ns = 0;
	rd->r = lfbm_oldest(b, w);
	return -EPIPE;
}

static inline uint64_t lfbm_now_ns(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* has the slot of the current ticket been uncommitted for too long? */
static inline int lfbm_stalled(lfbm_rd_t *rd)
{
	uint64_t now = lfbm_now_ns();

	if (rd->stall_ns == 0) {
		rd->stall_ns = now;
		return 0;
	}

	return now - rd->stall_ns >= rd->stall_limit_ns;
}

/**
 * @brief read the next frame
 *
 * There is no blocking read; poll with a caller-chosen strategy.
 *
 * @param rd the reader
 * @param frame destination, frame_size - LFBM_SEQ_SZ bytes
 * @return 1 if a frame was copied, 0 if there is nothing to deliver
 *         yet, -EPIPE after an overrun (data was lost; retry to
 *         continue)
 */
static inline int lfbm_read(lfbm_rd_t *rd, void *frame)
{
	const lfb_t *b = rd->b;

	for (;;) {
		uint64_t w = atomic_load_explicit(&b->w, memory_order_acquire);
		const _Atomic uint64_t *seq;
		uint64_t e;

		if (w == rd->r) {
			rd->stall_ns = 0;
			return 0;
		}

		if (w - rd->r >= b->depth)
			return lfbm_overrun(rd);

		seq = lfbm_cseq(b, rd->r);
		e = atomic_load_explicit(seq, memory_order_acquire);

		if (e == LFBM_DONE(rd->r)) {
			lfb_frame_copy(frame,
				       (const uint8_t *)seq + LFBM_SEQ_SZ,
				       b->frame_size - LFBM_SEQ_SZ);

			/* orders the payload loads before the re-load */
			atomic_thread_fence(memory_order_acquire);

			if (atomic_load_explicit(seq, memory_order_relaxed) != e)
				return lfbm_overrun(rd);

			rd->r++;
			rd->stall_ns = 0;
			return 1;
		}

		if (e > LFBM_BUSY(rd->r))
			return lfbm_overrun(rd);

		/*
		 * an older ticket being busy in our slot stalled for a
		 * whole lap, and ours was abandoned: there is nothing to
		 * wait for. The frame of the stalled writer is lost to
		 * us, count it once, not on every lap of a dead slot.
		 */
		if ((e & 1) != 0 && e != LFBM_BUSY(rd->r)) {
			if (rd->r >= b->depth &&
			    e == LFBM_BUSY(rd->r - b->depth) &&
			    rd->gave_up != rd->r - b->depth + 1)
				rd->skipped++;

			rd->r++;
			rd->stall_ns = 0;
			continue;
		}

		/* otherwise the writer is expected any moment */
		if (!lfbm_stalled(rd))
			return 0;

		rd->gave_up = ++rd->r;
		rd->skipped++;

		/*
		 * writers tend to stall together, e.g. the threads of a
		 * frozen process: don't wait the full limit for each,
		 * but do allow for the next ticket being in progress
		 */
		rd->stall_ns = lfbm_now_ns() - rd->stall_limit_ns +
			rd->stall_limit_ns / 16;
	}
}

/**
 * @brief distance from the read cursor to the write position
 *
 * @param rd the reader
 * @return lag in tickets, clamped to depth (which means overrun)
 */
static inline lfb_word_t lfbm_lag(const lfbm_rd_t *rd)
{
	const lfb_t *b = rd->b;
	uint64_t w = atomic_load_explicit(&b->w, memory_order_acquire);
	uint64_t lag = w - rd->r;

	return (lag > b->depth) ? b->depth : lag;
}

#endif /* _LFBM_H_ */
