/*
 * lfb.h - a minimal lock-free single-producer/multi-consumer
 *         broadcast ring buffer
 *
 * Copyright (C) 2026 Markus Klotzbuecher <mk@mkio.de>
 *
 * SPDX-License-Identifier: MPL-2.0
 */

/*
 * lfb implements a lossy broadcast ring buffer for one real-time
 * producer and any number of independent consumers, e.g. for
 * publishing log messages, samples or state from a real-time thread
 * to non real-time observers.
 *
 * Properties:
 *
 *  - the producer never blocks, never fails and never allocates: it
 *    overwrites the oldest frame when the buffer is full. The write
 *    path is a memcpy plus one atomic release-store.
 *
 *  - consumers maintain private read cursors and do not synchronize
 *    with the producer or each other. A consumer that falls more
 *    than `depth` frames behind loses data and is notified via
 *    -EPIPE (see lfb_read). Use lfb_lag to detect impending
 *    overruns early.
 *
 *  - the buffer operates on caller-provided memory (static, heap or
 *    shared memory). All shared state lives inside that region, so
 *    it works across processes, provided the atomics are
 *    address-free (statically asserted). See lfb_shm.h for an
 *    optional POSIX shm lifecycle layer.
 *
 *  - the payload region is contiguous: depth * frame_size opaque
 *    bytes, no per-frame headers. Slot i is located at byte offset
 *    i * frame_size, so choose frame_size as a multiple of the
 *    frame type's alignment (sizeof(struct ...) does this
 *    naturally).
 *
 *  - an optional caller-owned user area of user_sz bytes can be
 *    reserved between the header and the payload (see lfb_user),
 *    e.g. for application metadata or a process-shared lock
 *    serializing multiple writers. To initialize it before the
 *    buffer becomes visible to attachers, use the two-phase
 *    lfb_prepare/lfb_publish instead of the one-shot lfb_init.
 *
 * The write position is a single atomic word combining a wrap
 * counter and a slot offset:
 *
 *      [ wrap count | slot offset ]
 *        <- rest ->   <- LFB_OFF_BITS ->
 *
 * The split is compile-time configurable (define before including
 * lfb.h):
 *
 *   #define LFB_CTR_BITS 64   // 32 or 64: total width (default 64)
 *   #define LFB_OFF_BITS 32   // offset bits (default 32)
 *
 * On targets without lock-free 64-bit atomics (e.g. some 32-bit
 * ARMs), use LFB_CTR_BITS 32 with a smaller offset field, e.g.
 * LFB_OFF_BITS 16 (max depth 65536). Caveat: the wrap counter
 * aliases after 2^(LFB_CTR_BITS - LFB_OFF_BITS) laps of the ring; a
 * consumer stalled for longer than that may miss an overrun. Keep
 * at least 8 wrap bits (4 are statically enforced).
 *
 * Producer and all consumers must be compiled with the same
 * configuration; lfb_attach verifies this against a fingerprint
 * stored in the buffer header.
 *
 * Thread-safety: one concurrent writer (serialize externally for
 * more), any number of concurrent readers. A lfb_rd_t is private to
 * one reader thread.
 *
 * Concurrency note: lfb_read validates the copied frame against the
 * write position afterwards (seqlock-style), so torn frames are
 * detected and never delivered. The payload copy itself is a benign
 * data race by design; when compiled with ThreadSanitizer the copy
 * is performed with relaxed atomic byte accesses instead, making
 * the library TSAN-clean (see lfb_frame_copy).
 */

#ifndef _LFB_H_
#define _LFB_H_

#include <errno.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef LFB_CTR_BITS
#define LFB_CTR_BITS 64
#endif

#ifndef LFB_OFF_BITS
#define LFB_OFF_BITS 32
#endif

#if LFB_CTR_BITS == 64
typedef uint64_t lfb_word_t;
/*
 * w is accessed concurrently, possibly from multiple processes, so
 * the atomic must be address-free (a libatomic lock-based fallback
 * would only synchronize within one process)
 */
_Static_assert(ATOMIC_LLONG_LOCK_FREE == 2,
	       "no lock-free 64-bit atomics, use LFB_CTR_BITS 32");
#elif LFB_CTR_BITS == 32
typedef uint32_t lfb_word_t;
_Static_assert(ATOMIC_INT_LOCK_FREE == 2,
	       "no lock-free 32-bit atomics on this target");
#else
#error "LFB_CTR_BITS must be 32 or 64"
#endif

#define LFB_WRAP_BITS (LFB_CTR_BITS - LFB_OFF_BITS)

#if LFB_OFF_BITS < 1 || LFB_WRAP_BITS < 4
#error "invalid LFB_OFF_BITS: need >= 1 offset bits and >= 4 wrap bits"
#endif

#define LFB_OFF_MASK	((((lfb_word_t)1) << LFB_OFF_BITS) - 1)
#define LFB_WRAP_MASK	((((lfb_word_t)1) << LFB_WRAP_BITS) - 1)

/* maximum ring depth for this configuration */
#define LFB_MAX_DEPTH	(((lfb_word_t)1) << LFB_OFF_BITS)

/*
 * number of frames of headroom kept when (re)positioning a reader at
 * the oldest frame (LFB_OLDEST or after an overrun): without it, the
 * producer would immediately overrun the reader again while it
 * catches up
 */
#ifndef LFB_CRUSH
#define LFB_CRUSH(depth) ((depth) / 64 + 1)
#endif

#if defined(__SANITIZE_THREAD__)
#define LFB_TSAN 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define LFB_TSAN 1
#endif
#endif

/* whence values for lfb_seek */
#define LFB_OLDEST 0	/* start at the oldest safely readable frame */
#define LFB_NEWEST 1	/* skip history, only frames written from now on */

/**
 * lfb buffer header
 *
 * Placed by lfb_init at the start of the caller-provided memory
 * region and followed (at offset LFB_HDR_SZ) by depth * frame_size
 * bytes of payload. All fields are private to the library.
 *
 * magic is written last (release) by lfb_publish and checked first
 * (acquire) by lfb_attach, so attachers never see a half-written
 * header. It fingerprints the protocol version, the bit split and
 * the buffer geometry, so config or layout mismatches are rejected.
 */
typedef struct lfb {
	_Atomic uint32_t magic;
	uint32_t frame_size;
	uint32_t user_sz;
	lfb_word_t depth;
	_Atomic lfb_word_t w;	/* combined wrap|off write position:
				 * stored with release order after the
				 * frame is written, loaded with
				 * acquire order by readers */
} lfb_t;

/* round up to max_align_t alignment */
#define LFB_ALIGN_UP(x) \
	((((size_t)(x)) + _Alignof(max_align_t) - 1) & \
	 ~((size_t)_Alignof(max_align_t) - 1))

/* header size: user area and payload start max_align_t-aligned */
#define LFB_HDR_SZ LFB_ALIGN_UP(sizeof(lfb_t))

/**
 * per-consumer read state
 *
 * Lives in reader-private memory (not in the shared region). One
 * lfb_rd_t per reader thread; readers never write to the lfb_t, so
 * read-only mappings work.
 */
typedef struct lfb_rd {
	const lfb_t *b;
	lfb_word_t r;		/* combined wrap|off read cursor */
	unsigned long overruns;	/* nr of overruns since lfb_seek */
} lfb_rd_t;

/* internal helpers to handle the combined wrap|off word */

static inline lfb_word_t lfb_w_off(lfb_word_t w)
{
	return w & LFB_OFF_MASK;
}

static inline lfb_word_t lfb_w_wrap(lfb_word_t w)
{
	return w >> LFB_OFF_BITS;
}

/* advance w by one frame, handling the explicit wrap */
static inline lfb_word_t lfb_w_inc(lfb_word_t w, lfb_word_t depth)
{
	if (lfb_w_off(w) + 1 == depth)
		return ((lfb_w_wrap(w) + 1) & LFB_WRAP_MASK) << LFB_OFF_BITS;
	return w + 1;
}

/*
 * distance in frames from read position r to write position w. The
 * arithmetic is modular in the wrap field, so the result is exact as
 * long as the true distance is below 2^LFB_WRAP_BITS laps (see the
 * aliasing caveat above)
 */
static inline lfb_word_t lfb_w_lag(lfb_word_t w, lfb_word_t r,
				   lfb_word_t depth)
{
	lfb_word_t dwrap = (lfb_w_wrap(w) - lfb_w_wrap(r)) & LFB_WRAP_MASK;
	return dwrap * depth + lfb_w_off(w) - lfb_w_off(r);
}

/* step w back by n frames (n < depth), clamping at the origin */
static inline lfb_word_t lfb_w_back(lfb_word_t w, lfb_word_t n,
				    lfb_word_t depth)
{
	lfb_word_t off = lfb_w_off(w);
	lfb_word_t wrap = lfb_w_wrap(w);

	if (n <= off)
		return (wrap << LFB_OFF_BITS) | (off - n);
	if (wrap == 0)
		return 0;	/* fewer than n frames ever written */
	return (((wrap - 1) & LFB_WRAP_MASK) << LFB_OFF_BITS) |
		(off + depth - n);
}

/*
 * fingerprint the protocol version, bit configuration and buffer
 * geometry into the magic value (FNV-1a style mix). Never 0, which
 * means "uninitialized".
 */
static inline uint32_t lfb_fingerprint(uint32_t frame_size, lfb_word_t depth,
				       uint32_t user_sz)
{
	uint32_t h = 0x4c464200u | 1;	/* "LFB" + protocol version */

	h = (h ^ LFB_CTR_BITS) * 16777619u;
	h = (h ^ LFB_OFF_BITS) * 16777619u;
	h = (h ^ frame_size) * 16777619u;
	h = (h ^ user_sz) * 16777619u;
	h = (h ^ (uint32_t)depth) * 16777619u;
	h = (h ^ (uint32_t)((uint64_t)depth >> 32)) * 16777619u;

	return (h != 0) ? h : 1;
}

/*
 * copy a frame. Plain memcpy normally; under ThreadSanitizer use
 * relaxed atomic byte accesses so that the deliberate seqlock race
 * on the payload (which lfb_read's post-copy validation makes
 * harmless) does not show up as a report
 */
static inline void lfb_frame_copy(void *dst, const void *src, size_t len)
{
#ifdef LFB_TSAN
	uint8_t *d = (uint8_t *)dst;
	const uint8_t *s = (const uint8_t *)src;

	for (size_t i = 0; i < len; i++)
		__atomic_store_n(&d[i],
				 __atomic_load_n(&s[i], __ATOMIC_RELAXED),
				 __ATOMIC_RELAXED);
#else
	memcpy(dst, src, len);
#endif
}

static inline uint8_t *lfb_data(lfb_t *b)
{
	return (uint8_t *)b + LFB_HDR_SZ + LFB_ALIGN_UP(b->user_sz);
}

static inline const uint8_t *lfb_cdata(const lfb_t *b)
{
	return (const uint8_t *)b + LFB_HDR_SZ + LFB_ALIGN_UP(b->user_sz);
}

/**
 * @brief the caller-owned user area of the buffer
 *
 * The user area is a max_align_t-aligned region of user_sz bytes
 * between the header and the payload, reserved at lfb_prepare/lfb_init
 * time (and covered by the header fingerprint, so all parties agree
 * on its size). The library never touches it after lfb_prepare (which
 * zeroes it); layout and synchronization of its contents are
 * entirely the caller's, e.g. application metadata describing the
 * frames, or a process-shared robust mutex serializing multiple
 * writers.
 *
 * To initialize it *before* the buffer becomes attachable, create
 * the buffer with lfb_prepare, fill the user area, then lfb_publish.
 *
 * @param b the buffer
 * @return pointer to user_sz bytes, or NULL if user_sz is 0
 */
static inline void *lfb_user(lfb_t *b)
{
	return (b->user_sz > 0) ? (uint8_t *)b + LFB_HDR_SZ : NULL;
}

/** @brief const variant of lfb_user (for read-only mappings) */
static inline const void *lfb_cuser(const lfb_t *b)
{
	return (b->user_sz > 0) ? (const uint8_t *)b + LFB_HDR_SZ : NULL;
}

/**
 * @brief calculate the required memory region size
 *
 * Use this to size the shm segment / static buffer to pass to
 * lfb_init.
 *
 * @param frame_size size of one frame in bytes (> 0)
 * @param depth number of frames (>= 2, <= LFB_MAX_DEPTH, need not be
 *        a power of two)
 * @param user_sz size of the caller-owned user area in bytes (0 for
 *        none, see lfb_user)
 * @return required size in bytes, or 0 if the parameters are invalid
 */
static inline size_t lfb_memsz(uint32_t frame_size, lfb_word_t depth,
			       uint32_t user_sz)
{
	size_t hdr;

	if (frame_size == 0 || depth < 2 || depth > LFB_MAX_DEPTH)
		return 0;
	/* only relevant for 32-bit size_t */
	if ((uint64_t)user_sz + _Alignof(max_align_t) >
	    (uint64_t)(SIZE_MAX - LFB_HDR_SZ))
		return 0;

	hdr = LFB_HDR_SZ + LFB_ALIGN_UP(user_sz);

	if ((uint64_t)depth > (uint64_t)((SIZE_MAX - hdr) / frame_size))
		return 0;

	return hdr + (size_t)frame_size * (size_t)depth;
}

/**
 * @brief prepare a broadcast buffer without publishing it
 *
 * Producer-side, first half of the two-phase initialization: writes
 * the header with magic 0 ("uninitialized"), resets the write
 * position and zeroes the user area. Any previous contents are
 * discarded. lfb_attach fails (benignly) until lfb_publish is
 * called, giving the caller a window to initialize the user area
 * before the buffer becomes visible, e.g. to set up a
 * process-shared lock in it.
 *
 * memsz may be larger than required (e.g. a page-rounded shm
 * segment); the excess is unused.
 *
 * @param mem memory region, aligned for lfb_t (mmap and malloc
 *        satisfy this)
 * @param memsz size of the region, >= lfb_memsz(frame_size, depth,
 *        user_sz)
 * @param frame_size size of one frame in bytes
 * @param depth number of frames (see lfb_memsz for limits)
 * @param user_sz size of the caller-owned user area (0 for none)
 * @return the prepared buffer (== mem), or NULL if mem is NULL,
 *         misaligned, too small or the parameters are invalid
 */
static inline lfb_t *lfb_prepare(void *mem, size_t memsz, uint32_t frame_size,
			      lfb_word_t depth, uint32_t user_sz)
{
	lfb_t *b = (lfb_t *)mem;
	size_t sz = lfb_memsz(frame_size, depth, user_sz);

	if (b == NULL || sz == 0 || memsz < sz)
		return NULL;
	if (((uintptr_t)mem & (_Alignof(lfb_t) - 1)) != 0)
		return NULL;

	atomic_store_explicit(&b->magic, 0, memory_order_relaxed);
	b->frame_size = frame_size;
	b->depth = depth;
	b->user_sz = user_sz;
	atomic_store_explicit(&b->w, 0, memory_order_relaxed);

	if (user_sz > 0)
		memset((uint8_t *)b + LFB_HDR_SZ, 0, user_sz);

	return b;
}

/**
 * @brief publish a prepared buffer
 *
 * Second half of the two-phase initialization: release-stores the
 * header magic, making the buffer attachable. Call exactly once,
 * after the user area (if any) is set up.
 *
 * @param b a buffer returned by lfb_prepare
 */
static inline void lfb_publish(lfb_t *b)
{
	atomic_store_explicit(&b->magic,
			      lfb_fingerprint(b->frame_size, b->depth,
					      b->user_sz),
			      memory_order_release);
}

/**
 * @brief initialize a broadcast buffer in a caller-provided region
 *
 * One-shot convenience for lfb_prepare + lfb_publish: the buffer is
 * attachable (with a zeroed user area) on return. See lfb_prepare for
 * the parameters.
 *
 * @return the initialized buffer (== mem), or NULL as per lfb_prepare
 */
static inline lfb_t *lfb_init(void *mem, size_t memsz, uint32_t frame_size,
			      lfb_word_t depth, uint32_t user_sz)
{
	lfb_t *b = lfb_prepare(mem, memsz, frame_size, depth, user_sz);

	if (b != NULL)
		lfb_publish(b);

	return b;
}

/**
 * @brief attach to an initialized buffer
 *
 * Consumer-side. Validates the header fingerprint: magic, protocol
 * version, LFB_CTR_BITS/LFB_OFF_BITS split, frame_size, depth and
 * user_sz must match this compilation, and the region must be large
 * enough. Use this to safely reject uninitialized, stale or foreign
 * memory (e.g. an old-layout shm segment).
 *
 * @param mem memory region containing a buffer initialized by
 *        lfb_init (typically a shm mapping, read-only is fine)
 * @param memsz size of the region
 * @return the buffer (== mem), or NULL on any mismatch
 */
static inline lfb_t *lfb_attach(void *mem, size_t memsz)
{
	lfb_t *b = (lfb_t *)mem;
	uint32_t magic;
	size_t sz;

	if (b == NULL || memsz < LFB_HDR_SZ)
		return NULL;
	if (((uintptr_t)mem & (_Alignof(lfb_t) - 1)) != 0)
		return NULL;

	magic = atomic_load_explicit(&b->magic, memory_order_acquire);

	if (magic == 0 ||
	    magic != lfb_fingerprint(b->frame_size, b->depth, b->user_sz))
		return NULL;

	sz = lfb_memsz(b->frame_size, b->depth, b->user_sz);

	if (sz == 0 || memsz < sz)
		return NULL;

	return b;
}

/**
 * @brief get the next write slot for in-place filling (zero-copy)
 *
 * Returns a pointer to the slot that the next lfb_commit_wslot will
 * publish. The caller may construct the frame directly in the slot
 * and must then call lfb_commit_wslot to make it visible to readers. Until
 * lfb_commit_wslot, readers do not deliver the slot (a maximally lagging
 * reader may be concurrently copying the slot's previous
 * generation; its post-copy validation discards that read).
 *
 * Note: in-place filling constitutes the seqlock payload race (see
 * lfb_frame_copy); prefer lfb_write in ThreadSanitizer builds.
 *
 * @param b the buffer
 * @return pointer to frame_size writable bytes
 */
static inline void *lfb_get_wslot(lfb_t *b)
{
	lfb_word_t w = atomic_load_explicit(&b->w, memory_order_relaxed);

	return lfb_data(b) + lfb_w_off(w) * (size_t)b->frame_size;
}

/**
 * @brief publish the slot obtained via lfb_get_wslot
 *
 * Advances the write position with release semantics. Call exactly
 * once per lfb_get_wslot.
 *
 * @param b the buffer
 */
static inline void lfb_commit_wslot(lfb_t *b)
{
	lfb_word_t w = atomic_load_explicit(&b->w, memory_order_relaxed);

	atomic_store_explicit(&b->w, lfb_w_inc(w, b->depth),
			      memory_order_release);
}

/**
 * @brief write a frame
 *
 * Copies frame_size bytes into the next slot and publishes it with a
 * release-store of the write position. Never blocks, never fails; if
 * the buffer is full, the oldest frame is overwritten. Not
 * thread-safe: one producer only, or serialize externally.
 *
 * @param b the buffer
 * @param frame frame to write (frame_size bytes are copied)
 */
static inline void lfb_write(lfb_t *b, const void *frame)
{
	lfb_frame_copy(lfb_get_wslot(b), frame, b->frame_size);
	lfb_commit_wslot(b);
}

/**
 * @brief seek a read cursor to the oldest or newest frame
 *
 * Positions @rd (and initializes it on the first call). whence
 * selects the starting position:
 *
 *  - LFB_NEWEST: at the current write position; only frames written
 *    after this call are delivered.
 *
 *  - LFB_OLDEST: at the oldest frame that can be read without an
 *    immediate overrun, i.e. at a lag of depth minus a small safety
 *    margin (LFB_CRUSH), which accounts for frames the producer may
 *    overwrite while the reader catches up.
 *
 * May be called again at any time to resync, e.g. when lfb_lag
 * reports that the reader is about to fall behind.
 *
 * @param b an attached buffer
 * @param rd reader state to position (reader-private memory)
 * @param whence LFB_OLDEST or LFB_NEWEST
 */
static inline void lfb_seek(const lfb_t *b, lfb_rd_t *rd, int whence)
{
	lfb_word_t w = atomic_load_explicit(&b->w, memory_order_acquire);

	rd->b = b;
	rd->overruns = 0;
	rd->r = (whence == LFB_NEWEST) ?
		w : lfb_w_back(w, b->depth - LFB_CRUSH(b->depth), b->depth);
}

/**
 * @brief borrow the next frame in place, without copying
 *
 * Zero-copy alternative to lfb_read, for frames large enough that
 * the copy is worth avoiding. Points @frame at the slot inside the
 * buffer and leaves the read cursor where it is; the borrow ends at
 * the matching lfb_check_rslot, which validates it.
 *
 * The producer does not know about the borrow and may overwrite the
 * slot at any time while the caller reads it. Everything the caller
 * derives from the frame is therefore *provisional* until
 * lfb_check_rslot returns 1:
 *
 *   const void *f;
 *
 *   if (lfb_get_rslot(&rd, &f) == 1) {
 *           n = format(scratch, f);          // provisional
 *
 *           if (lfb_check_rslot(&rd) == 1)
 *                   emit(scratch, n);        // now known good
 *   }
 *
 * Do not act irreversibly on the contents before lfb_check_rslot
 * confirms them: write into a scratch buffer or compute into locals, and
 * publish only afterwards. Note that a torn frame can hold
 * arbitrary bytes, so a caller that follows pointers, indexes
 * arrays or loops on a length taken from the frame must bounds-check
 * it -- validation happens after the fact, and cannot undo a crash.
 * Reading only within frame_size bytes is always safe.
 *
 * When frames are small, prefer lfb_read: it is simpler, has the
 * same cost within a memcpy, and cannot be misused this way.
 *
 * The pointer stays valid until the next operation on @rd. Under
 * ThreadSanitizer the caller's own loads from the frame are
 * uninstrumented and will be reported as races on the payload (see
 * the concurrency note at the top); lfb_read has no such problem.
 *
 * @param rd the reader
 * @param frame out-value, set to the frame inside the buffer
 * @return 1 if a frame was borrowed (call lfb_check_rslot when
 *         done), 0 if no new data, -EPIPE after an overrun (nothing
 *         was borrowed and no lfb_check_rslot is owed; retry to continue)
 */
static inline int lfb_get_rslot(lfb_rd_t *rd, const void **frame)
{
	const lfb_t *b = rd->b;
	lfb_word_t w, lag;

	w = atomic_load_explicit(&b->w, memory_order_acquire);
	lag = lfb_w_lag(w, rd->r, b->depth);

	if (lag == 0)
		return 0;

	if (lag >= b->depth) {
		rd->overruns++;
		rd->r = lfb_w_back(w, b->depth - LFB_CRUSH(b->depth),
				   b->depth);
		return -EPIPE;
	}

	*frame = lfb_cdata(b) + lfb_w_off(rd->r) * (size_t)b->frame_size;
	return 1;
}

/**
 * @brief end a borrow started by lfb_get_rslot and validate it
 *
 * Seqlock-style validation: if the producer entered the borrowed
 * slot while the caller was reading it, the data may be torn. On
 * success the read cursor advances to the next frame.
 *
 * Call exactly once per lfb_get_rslot that returned 1.
 *
 * @param rd the reader
 * @return 1 if the frame was intact for the whole borrow (the data
 *         the caller read is good), -EPIPE if it was overwritten:
 *         discard everything derived from it, the cursor is resynced
 *         as per LFB_OLDEST and rd->overruns incremented
 */
static inline int lfb_check_rslot(lfb_rd_t *rd)
{
	const lfb_t *b = rd->b;
	lfb_word_t w;

	/*
	 * the acquire fence orders the caller's payload loads before
	 * the re-load of w below
	 */
	atomic_thread_fence(memory_order_acquire);
	w = atomic_load_explicit(&b->w, memory_order_relaxed);

	if (lfb_w_lag(w, rd->r, b->depth) >= b->depth) {
		rd->overruns++;
		rd->r = lfb_w_back(w, b->depth - LFB_CRUSH(b->depth),
				   b->depth);
		return -EPIPE;
	}

	rd->r = lfb_w_inc(rd->r, b->depth);
	return 1;
}

/**
 * @brief read the next frame
 *
 * Copies the frame at the read cursor into @frame and advances the
 * cursor. The copy is validated against the write position
 * afterwards (seqlock-style), so a frame the producer overwrote
 * mid-copy is never delivered.
 *
 * On overrun (the producer lapped the cursor, before or during the
 * copy) the affected frames are lost: the cursor is resynced as per
 * LFB_OLDEST, rd->overruns is incremented and -EPIPE returned. The
 * next call delivers the oldest surviving frame.
 *
 * There is no blocking read; poll with a caller-chosen strategy.
 * See lfb_get_rslot to avoid the copy for large frames.
 *
 * @param rd the reader
 * @param frame destination, frame_size bytes
 * @return 1 if a frame was copied, 0 if no new data, -EPIPE after an
 *         overrun (data was lost; retry to continue)
 */
static inline int lfb_read(lfb_rd_t *rd, void *frame)
{
	const void *src;
	int ret = lfb_get_rslot(rd, &src);

	if (ret != 1)
		return ret;

	lfb_frame_copy(frame, src, rd->b->frame_size);

	return lfb_check_rslot(rd);
}

/**
 * @brief distance from the read cursor to the write position
 *
 * Returns the number of unread frames, i.e. how far this reader
 * trails the producer. The value is a snapshot and can only have
 * grown by the time it is used; 0 means "was empty".
 *
 * Use this to avoid falling behind: the reader is overrun when the
 * lag reaches depth, so the remaining headroom is depth -
 * lfb_lag(rd). A reader may e.g. batch-drain without per-frame
 * processing, shorten its poll interval, or voluntarily resync via
 * lfb_seek once the lag exceeds a threshold (say depth/2), and
 * thereby choose *which* data to lose instead of being overrun at a
 * random point.
 *
 * @param rd the reader
 * @return lag in frames, clamped to depth (i.e. depth means "at
 *         least an overrun away")
 */
static inline lfb_word_t lfb_lag(const lfb_rd_t *rd)
{
	const lfb_t *b = rd->b;
	lfb_word_t w = atomic_load_explicit(&b->w, memory_order_acquire);
	lfb_word_t lag = lfb_w_lag(w, rd->r, b->depth);

	return (lag > b->depth) ? b->depth : lag;
}

#endif /* _LFB_H_ */
