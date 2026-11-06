/*
 * test-lfbm-unit.c - cmocka unit tests for the lfbm multi-producer ring
 *
 * Copyright (C) 2026 Markus Klotzbuecher <mk@mkio.de>
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdlib.h>
#include <string.h>

/* don't wait a second for each stalled slot */
#define LFBM_STALL_NS 1000000ULL

#include "lfbm.h"

#define PAYLOAD_SZ	((uint32_t)sizeof(uint64_t))
#define DEPTH		8
#define POLL_LIMIT	100000000

static void *mem;

static lfb_t *mk_buf(void)
{
	size_t sz = lfbm_memsz(PAYLOAD_SZ, DEPTH, 0);
	lfb_t *b;

	/* lfbm_init must not depend on zeroed memory */
	mem = malloc(sz);
	assert_non_null(mem);
	memset(mem, 0xa5, sz);
	b = lfbm_init(mem, sz, PAYLOAD_SZ, DEPTH, 0);
	assert_non_null(b);
	return b;
}

static void wr(lfb_t *b, uint64_t v)
{
	assert_int_equal(lfbm_write(b, &v), 0);
}

static void rd_expect(lfbm_rd_t *rd, uint64_t v)
{
	uint64_t f = 0;

	assert_int_equal(lfbm_read(rd, &f), 1);
	assert_int_equal(f, v);
}

/* take a ticket and hold its slot, like a writer preempted mid-copy */
static uint64_t claim(lfb_t *b)
{
	uint64_t t = atomic_fetch_add(&b->w, 1);

	atomic_store(lfbm_seq(b, t), LFBM_BUSY(t));
	return t;
}

static void test_write_read(void **state)
{
	lfb_t *b = mk_buf();
	lfbm_rd_t rd;
	uint64_t f;

	(void)state;
	lfbm_rd_init(b, &rd, LFB_OLDEST);
	assert_int_equal(lfbm_read(&rd, &f), 0);

	wr(b, 10);
	wr(b, 11);
	assert_int_equal(lfbm_lag(&rd), 2);
	rd_expect(&rd, 10);
	rd_expect(&rd, 11);
	assert_int_equal(lfbm_read(&rd, &f), 0);
	assert_int_equal(lfbm_lag(&rd), 0);
	free(mem);
}

static void test_newest(void **state)
{
	lfb_t *b = mk_buf();
	lfbm_rd_t rd;

	(void)state;
	wr(b, 1);
	lfbm_rd_init(b, &rd, LFB_NEWEST);
	wr(b, 2);
	rd_expect(&rd, 2);
	free(mem);
}

static void test_wrap(void **state)
{
	lfb_t *b = mk_buf();
	lfbm_rd_t rd;

	(void)state;
	lfbm_rd_init(b, &rd, LFB_NEWEST);

	for (uint64_t i = 0; i < 5 * DEPTH; i++) {
		wr(b, i);
		rd_expect(&rd, i);
	}

	assert_int_equal(rd.overruns, 0);
	assert_int_equal(rd.skipped, 0);
	free(mem);
}

/* a full ring overwrites the oldest frames, reader resyncs to survivors */
static void test_overwrite(void **state)
{
	lfb_t *b = mk_buf();
	lfbm_rd_t rd;
	uint64_t f;

	(void)state;
	lfbm_rd_init(b, &rd, LFB_NEWEST);

	for (uint64_t i = 0; i < 3 * DEPTH; i++)
		wr(b, i);

	assert_int_equal(lfbm_lag(&rd), DEPTH);
	assert_int_equal(lfbm_read(&rd, &f), -EPIPE);
	assert_int_equal(rd.overruns, 1);
	rd_expect(&rd, 3 * DEPTH - (DEPTH - LFB_CRUSH(DEPTH)));
	free(mem);
}

/* writers step over a slot held by a stalled writer, and don't touch it */
static void test_writer_skips_busy(void **state)
{
	lfb_t *b = mk_buf();
	uint64_t t = claim(b);
	uint64_t i;

	(void)state;

	for (i = 0; i < 3 * DEPTH; i++)
		wr(b, i);

	assert_int_equal(atomic_load(lfbm_seq(b, t)), LFBM_BUSY(t));
	free(mem);
}

/* a writer resuming after being lapped must abandon its ticket */
static void test_lapped_writer(void **state)
{
	lfb_t *b = mk_buf();
	uint64_t t = atomic_fetch_add(&b->w, 1);
	_Atomic uint64_t *seq = lfbm_seq(b, t);
	uint64_t newer = LFBM_DONE(t + DEPTH);

	(void)state;
	atomic_store(seq, newer);

	/* what the resumed writer sees: only its next ticket is free */
	atomic_store(&b->w, t);
	wr(b, 7);
	assert_int_equal(atomic_load(seq), newer);
	assert_int_equal(atomic_load(&b->w), t + 2);
	free(mem);
}

static void test_all_busy(void **state)
{
	lfb_t *b = mk_buf();
	uint64_t f = 0;

	(void)state;

	for (int i = 0; i < DEPTH; i++)
		claim(b);

	assert_int_equal(lfbm_write(b, &f), -EBUSY);
	free(mem);
}

/* the reader waits for an uncommitted slot, delivers once committed */
static void test_read_waits(void **state)
{
	lfb_t *b = mk_buf();
	lfbm_rd_t rd;
	uint64_t f, v = 42, t;

	(void)state;
	lfbm_rd_init(b, &rd, LFB_NEWEST);
	t = claim(b);
	wr(b, 43);

	assert_int_equal(lfbm_read(&rd, &f), 0);

	memcpy((uint8_t *)lfbm_seq(b, t) + LFBM_SEQ_SZ, &v, sizeof(v));
	atomic_store(lfbm_seq(b, t), LFBM_DONE(t));

	rd_expect(&rd, 42);
	rd_expect(&rd, 43);
	assert_int_equal(rd.skipped, 0);
	free(mem);
}

/* ... and gives up on it eventually, in order not to stop for good */
static void test_read_stalled(void **state)
{
	lfb_t *b = mk_buf();
	lfbm_rd_t rd;
	uint64_t f = 0;
	int ret = 0;

	(void)state;
	lfbm_rd_init(b, &rd, LFB_NEWEST);
	claim(b);
	wr(b, 43);

	for (int i = 0; i < POLL_LIMIT && ret == 0; i++)
		ret = lfbm_read(&rd, &f);

	assert_int_equal(ret, 1);
	assert_int_equal(f, 43);
	assert_int_equal(rd.skipped, 1);
	free(mem);
}

/* a ticket abandoned for a stalled older writer: no need to wait */
static void test_read_abandoned(void **state)
{
	lfb_t *b = mk_buf();
	lfbm_rd_t rd;
	uint64_t t = claim(b);

	(void)state;
	atomic_store(&b->w, t + DEPTH);
	lfbm_rd_init(b, &rd, LFB_NEWEST);
	rd.stall_limit_ns = UINT64_MAX;
	wr(b, 1);		/* steps over ticket t + DEPTH */
	assert_int_equal(lfbm_lag(&rd), 2);
	rd_expect(&rd, 1);

	/* the stalled writer's frame is lost to us: reported once */
	assert_int_equal(rd.skipped, 1);

	atomic_store(&b->w, t + 2 * DEPTH);
	lfbm_rd_init(b, &rd, LFB_NEWEST);
	rd.stall_limit_ns = UINT64_MAX;
	wr(b, 2);		/* steps over ticket t + 2 * DEPTH */
	rd_expect(&rd, 2);
	assert_int_equal(rd.skipped, 0);
	free(mem);
}

/* a dead slot is lost once, also to a reader that waited for it */
static void test_dead_slot_counted_once(void **state)
{
	lfb_t *b = mk_buf();
	lfbm_rd_t rd;
	uint64_t f = 0;

	(void)state;
	lfbm_rd_init(b, &rd, LFB_NEWEST);
	claim(b);

	for (int i = 0; i < POLL_LIMIT && rd.skipped == 0; i++)
		lfbm_read(&rd, &f);

	assert_int_equal(rd.skipped, 1);

	/* next lap: writers step over the dead slot, as does the reader */
	for (uint64_t i = 0; i < DEPTH - 1; i++) {
		wr(b, i);
		rd_expect(&rd, i);
	}

	wr(b, 100);		/* burns the dead slot's ticket, then writes */
	rd_expect(&rd, 100);
	assert_int_equal(rd.skipped, 1);

	free(mem);
}

/* a stale timer must not cost the next frame in progress */
static void test_stall_timer_cleared(void **state)
{
	lfb_t *b = mk_buf();
	lfbm_rd_t rd;
	uint64_t f = 0;

	(void)state;
	lfbm_rd_init(b, &rd, LFB_NEWEST);
	rd.stall_limit_ns = 1000;
	claim(b);

	while (rd.skipped == 0)
		lfbm_read(&rd, &f);

	assert_int_equal(lfbm_read(&rd, &f), 0);	/* r == w */
	assert_int_equal(rd.stall_ns, 0);

	rd.stall_limit_ns = UINT64_MAX;
	claim(b);
	assert_int_equal(lfbm_read(&rd, &f), 0);
	assert_int_equal(rd.skipped, 1);
	free(mem);
}

/* a seek keeps the counters and the stall limit */
static void test_seek_keeps_state(void **state)
{
	lfb_t *b = mk_buf();
	lfbm_rd_t rd;

	(void)state;
	lfbm_rd_init(b, &rd, LFB_NEWEST);
	rd.stall_limit_ns = 42;
	rd.skipped = 3;
	lfbm_seek(b, &rd, LFB_OLDEST);
	assert_int_equal(rd.stall_limit_ns, 42);
	assert_int_equal(rd.skipped, 3);
	free(mem);
}

/* several stalled tickets must not cost the full limit each */
static void test_stall_series(void **state)
{
	lfb_t *b = mk_buf();
	lfbm_rd_t rd;
	uint64_t f = 0, t0;
	int ret = 0;

	(void)state;
	lfbm_rd_init(b, &rd, LFB_NEWEST);
	rd.stall_limit_ns = 16000000ULL;

	for (int i = 0; i < 6; i++)
		claim(b);

	wr(b, 43);
	t0 = lfbm_now_ns();

	while (ret == 0)
		ret = lfbm_read(&rd, &f);

	assert_int_equal(ret, 1);
	assert_int_equal(f, 43);
	assert_int_equal(rd.skipped, 6);
	assert_true(lfbm_now_ns() - t0 < 3 * rd.stall_limit_ns);
	free(mem);
}

/* the stalled writer commits for the tickets abandoned because of it */
static void test_lapped_commit(void **state)
{
	lfb_t *b = mk_buf();
	lfbm_rd_t rd;
	uint64_t v = 42, t = claim(b);

	(void)state;
	atomic_store(&b->w, t + 2 * DEPTH + 1);
	lfbm_rd_init(b, &rd, LFB_NEWEST);
	rd.r = t + 2 * DEPTH;
	rd.stall_limit_ns = UINT64_MAX;

	memcpy((uint8_t *)lfbm_seq(b, t) + LFBM_SEQ_SZ, &v, sizeof(v));
	lfbm_commit(b, lfbm_seq(b, t), t);

	assert_int_equal(atomic_load(lfbm_seq(b, t)), LFBM_DONE(t + 2 * DEPTH));
	rd_expect(&rd, 42);
	assert_int_equal(rd.skipped, 0);
	assert_int_equal(rd.overruns, 0);
	free(mem);
}

/* plain and multi-producer rings refuse each other */
static void test_modes(void **state)
{
	size_t sz = lfbm_memsz(PAYLOAD_SZ, DEPTH, 0);
	void *m = malloc(sz);

	(void)state;
	assert_non_null(m);
	assert_non_null(lfbm_init(m, sz, PAYLOAD_SZ, DEPTH, 0));
	assert_true(lfb_attach(m, sz) == NULL);
	assert_non_null(lfbm_attach(m, sz));

	assert_non_null(lfb_init(m, sz, PAYLOAD_SZ, DEPTH, 0));
	assert_true(lfbm_attach(m, sz) == NULL);
	assert_non_null(lfb_attach(m, sz));
	free(m);
}

static void test_bad_payload(void **state)
{
	char m[1024];

	(void)state;
	assert_int_equal(lfbm_memsz(12, DEPTH, 0), 0);
	assert_int_equal(lfbm_memsz(0, DEPTH, 0), 0);
	assert_true(lfbm_init(m, sizeof(m), 12, DEPTH, 0) == NULL);

	/* the depth must be a power of two */
	assert_int_equal(lfbm_memsz(PAYLOAD_SZ, 12, 0), 0);
	assert_true(lfbm_init(m, sizeof(m), PAYLOAD_SZ, 12, 0) == NULL);
}

/* a frame overwritten while the reader got to it */
static void test_read_overwritten(void **state)
{
	lfb_t *b = mk_buf();
	lfbm_rd_t rd;
	uint64_t f;

	(void)state;
	lfbm_rd_init(b, &rd, LFB_NEWEST);
	wr(b, 1);
	atomic_store(lfbm_seq(b, 0), LFBM_DONE(DEPTH));

	assert_int_equal(lfbm_read(&rd, &f), -EPIPE);
	assert_int_equal(rd.overruns, 1);

	/* w is below the frames to keep: no underflow of the cursor */
	assert_int_equal(rd.r, 0);
	free(mem);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_write_read),
		cmocka_unit_test(test_newest),
		cmocka_unit_test(test_wrap),
		cmocka_unit_test(test_overwrite),
		cmocka_unit_test(test_writer_skips_busy),
		cmocka_unit_test(test_lapped_writer),
		cmocka_unit_test(test_all_busy),
		cmocka_unit_test(test_read_waits),
		cmocka_unit_test(test_read_stalled),
		cmocka_unit_test(test_read_abandoned),
		cmocka_unit_test(test_dead_slot_counted_once),
		cmocka_unit_test(test_stall_timer_cleared),
		cmocka_unit_test(test_seek_keeps_state),
		cmocka_unit_test(test_stall_series),
		cmocka_unit_test(test_lapped_commit),
		cmocka_unit_test(test_modes),
		cmocka_unit_test(test_bad_payload),
		cmocka_unit_test(test_read_overwritten),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
