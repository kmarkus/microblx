/*
 * test-lfb-unit.c - cmocka unit tests for the lfb core
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

#include "lfb.h"

typedef struct {
	uint32_t seq;
	uint32_t val;
} tframe_t;

#define TF_SZ ((uint32_t)sizeof(tframe_t))

/* allocate a region and initialize a buffer of the given depth */
static lfb_t *mk_buf(lfb_word_t depth, void **mem)
{
	size_t sz = lfb_memsz(TF_SZ, depth, 0);
	lfb_t *b;

	assert_true(sz > 0);
	*mem = malloc(sz);
	assert_non_null(*mem);
	b = lfb_init(*mem, sz, TF_SZ, depth, 0);
	assert_non_null(b);
	return b;
}

static void wr(lfb_t *b, uint32_t seq)
{
	tframe_t f = { .seq = seq, .val = seq * 3 };
	lfb_write(b, &f);
}

/* read one frame, expecting success and the given seq */
static void rd_expect(lfb_rd_t *rd, uint32_t seq)
{
	tframe_t f;

	assert_int_equal(lfb_read(rd, &f), 1);
	assert_int_equal(f.seq, seq);
	assert_int_equal(f.val, seq * 3);
}

static void test_memsz(void **state)
{
	(void)state;

	assert_int_equal(lfb_memsz(0, 8, 0), 0);
	assert_int_equal(lfb_memsz(8, 0, 0), 0);
	assert_int_equal(lfb_memsz(8, 1, 0), 0);

	assert_int_equal(lfb_memsz(8, 16, 0), LFB_HDR_SZ + 8 * 16);

	/* the user area is reserved max_align_t-aligned */
	assert_int_equal(lfb_memsz(8, 16, 24),
			 LFB_HDR_SZ + LFB_ALIGN_UP(24) + 8 * 16);

	assert_int_equal(lfb_memsz(8, LFB_MAX_DEPTH + 1, 0), 0);
}

static void test_init_invalid(void **state)
{
	size_t sz = lfb_memsz(TF_SZ, 16, 0);
	void *mem = malloc(sz);

	(void)state;
	assert_non_null(mem);

	assert_null(lfb_init(NULL, sz, TF_SZ, 16, 0));
	assert_null(lfb_init(mem, sz - 1, TF_SZ, 16, 0));
	assert_null(lfb_init(mem, sz, 0, 16, 0));
	assert_null(lfb_init(mem, sz, TF_SZ, 1, 0));

	/* memsz does not cover the requested user area */
	assert_null(lfb_init(mem, sz, TF_SZ, 16, 24));

	assert_ptr_equal(lfb_init(mem, sz, TF_SZ, 16, 0), mem);
	free(mem);
}

static void test_attach(void **state)
{
	size_t sz = lfb_memsz(TF_SZ, 16, 0);
	void *mem = calloc(1, sz);
	lfb_t *b;

	(void)state;
	assert_non_null(mem);

	/* zeroed region: magic 0 -> uninitialized */
	assert_null(lfb_attach(mem, sz));

	b = lfb_init(mem, sz, TF_SZ, 16, 0);
	assert_non_null(b);
	assert_ptr_equal(lfb_attach(mem, sz), b);

	/* region too small for the declared geometry */
	assert_null(lfb_attach(mem, sz - 1));

	/* geometry corrupted -> fingerprint mismatch */
	b->frame_size++;
	assert_null(lfb_attach(mem, sz));
	b->frame_size--;
	assert_non_null(lfb_attach(mem, sz));

	b->user_sz++;
	assert_null(lfb_attach(mem, sz));
	b->user_sz--;
	assert_non_null(lfb_attach(mem, sz));

	free(mem);
}

static void test_write_read(void **state)
{
	void *mem;
	lfb_t *b = mk_buf(8, &mem);
	lfb_rd_t rd;
	tframe_t f;

	(void)state;

	lfb_seek(b, &rd, LFB_OLDEST);
	assert_int_equal(lfb_read(&rd, &f), 0);

	for (uint32_t i = 0; i < 5; i++)
		wr(b, i);

	for (uint32_t i = 0; i < 5; i++)
		rd_expect(&rd, i);

	assert_int_equal(lfb_read(&rd, &f), 0);

	/* a late reader with LFB_OLDEST sees the partial history too */
	lfb_seek(b, &rd, LFB_OLDEST);
	for (uint32_t i = 0; i < 5; i++)
		rd_expect(&rd, i);
	assert_int_equal(lfb_read(&rd, &f), 0);

	free(mem);
}

/* get_rslot/commit delivers the same sequence as lfb_read */
static void test_rslot_commit(void **state)
{
	void *mem;
	lfb_t *b = mk_buf(8, &mem);
	lfb_rd_t rd;
	const void *f = NULL;

	(void)state;

	lfb_seek(b, &rd, LFB_OLDEST);
	assert_int_equal(lfb_get_rslot(&rd, &f), 0);

	for (uint32_t i = 0; i < 5; i++)
		wr(b, i);

	for (uint32_t i = 0; i < 5; i++) {
		const tframe_t *tf;

		assert_int_equal(lfb_get_rslot(&rd, &f), 1);
		tf = (const tframe_t *)f;
		assert_int_equal(tf->seq, i);
		assert_int_equal(tf->val, i * 3);

		/* the borrow points into the buffer, not at a copy */
		assert_true((const uint8_t *)f >= lfb_cdata(b));
		assert_true((const uint8_t *)f < lfb_cdata(b) + 8 * TF_SZ);

		assert_int_equal(lfb_check_rslot(&rd), 1);
	}

	assert_int_equal(lfb_get_rslot(&rd, &f), 0);
	assert_int_equal(rd.overruns, 0);
	free(mem);
}

/* get_rslot without commit does not advance: the same frame comes again */
static void test_rslot_no_commit(void **state)
{
	void *mem;
	lfb_t *b = mk_buf(8, &mem);
	lfb_rd_t rd;
	const void *f1 = NULL, *f2 = NULL;

	(void)state;

	lfb_seek(b, &rd, LFB_OLDEST);
	wr(b, 42);

	assert_int_equal(lfb_get_rslot(&rd, &f1), 1);
	assert_int_equal(((const tframe_t *)f1)->seq, 42);

	assert_int_equal(lfb_get_rslot(&rd, &f2), 1);
	assert_ptr_equal(f1, f2);

	assert_int_equal(lfb_check_rslot(&rd), 1);
	assert_int_equal(lfb_get_rslot(&rd, &f2), 0);

	free(mem);
}

/*
 * a producer lapping the reader during the borrow must be caught by
 * the commit, not by the get_rslot: this is the case the copy-free path
 * exists to detect
 */
static void test_rslot_overrun_during_borrow(void **state)
{
	void *mem;
	lfb_t *b = mk_buf(4, &mem);
	lfb_rd_t rd;
	const void *f = NULL;

	(void)state;

	wr(b, 0);
	lfb_seek(b, &rd, LFB_OLDEST);

	/* borrow succeeds while the frame is still live */
	assert_int_equal(lfb_get_rslot(&rd, &f), 1);
	assert_int_equal(((const tframe_t *)f)->seq, 0);

	/* producer laps the whole ring while we hold the borrow */
	for (uint32_t i = 1; i <= 8; i++)
		wr(b, i);

	assert_int_equal(lfb_check_rslot(&rd), -EPIPE);
	assert_int_equal(rd.overruns, 1);

	/* and the reader is resynced to the surviving history */
	assert_int_equal(lfb_get_rslot(&rd, &f), 1);
	assert_int_equal(lfb_check_rslot(&rd), 1);

	free(mem);
}

/* an overrun detected by get_rslot itself owes no commit */
static void test_rslot_overrun_before_borrow(void **state)
{
	void *mem;
	lfb_t *b = mk_buf(4, &mem);
	lfb_rd_t rd;
	const void *f = NULL;

	(void)state;

	lfb_seek(b, &rd, LFB_OLDEST);

	for (uint32_t i = 0; i < 20; i++)
		wr(b, i);

	assert_int_equal(lfb_get_rslot(&rd, &f), -EPIPE);
	assert_int_equal(rd.overruns, 1);

	assert_int_equal(lfb_get_rslot(&rd, &f), 1);
	assert_int_equal(lfb_check_rslot(&rd), 1);

	free(mem);
}

/* get_rslot and read are interchangeable on the same cursor */
static void test_rslot_read_mixed(void **state)
{
	void *mem;
	lfb_t *b = mk_buf(8, &mem);
	lfb_rd_t rd;
	const void *f = NULL;

	(void)state;

	lfb_seek(b, &rd, LFB_OLDEST);

	for (uint32_t i = 0; i < 6; i++)
		wr(b, i);

	rd_expect(&rd, 0);

	assert_int_equal(lfb_get_rslot(&rd, &f), 1);
	assert_int_equal(((const tframe_t *)f)->seq, 1);
	assert_int_equal(lfb_check_rslot(&rd), 1);

	rd_expect(&rd, 2);

	assert_int_equal(lfb_get_rslot(&rd, &f), 1);
	assert_int_equal(((const tframe_t *)f)->seq, 3);
	assert_int_equal(lfb_check_rslot(&rd), 1);

	rd_expect(&rd, 4);
	rd_expect(&rd, 5);

	assert_int_equal(rd.overruns, 0);
	free(mem);
}

static void test_newest(void **state)
{
	void *mem;
	lfb_t *b = mk_buf(8, &mem);
	lfb_rd_t rd;
	tframe_t f;

	(void)state;

	for (uint32_t i = 0; i < 3; i++)
		wr(b, i);

	lfb_seek(b, &rd, LFB_NEWEST);
	assert_int_equal(lfb_read(&rd, &f), 0);

	wr(b, 99);
	rd_expect(&rd, 99);
	assert_int_equal(lfb_read(&rd, &f), 0);

	free(mem);
}

static void test_wrap(void **state)
{
	void *mem;
	lfb_t *b = mk_buf(4, &mem);
	lfb_rd_t rd;

	(void)state;

	lfb_seek(b, &rd, LFB_OLDEST);

	/* interleaved write/read crossing the wrap many times */
	for (uint32_t i = 0; i < 100; i++) {
		wr(b, i);
		rd_expect(&rd, i);
	}

	assert_int_equal(rd.overruns, 0);
	free(mem);
}

static void test_overrun_resync(void **state)
{
	void *mem;
	lfb_t *b = mk_buf(8, &mem);
	lfb_rd_t rd;
	tframe_t f;

	(void)state;

	lfb_seek(b, &rd, LFB_OLDEST);

	for (uint32_t i = 0; i < 20; i++)
		wr(b, i);

	/* reader was lapped: overrun, resync to depth - LFB_CRUSH(8) =
	 * 7 frames back from w=20, i.e. seq 13 */
	assert_int_equal(lfb_read(&rd, &f), -EPIPE);
	assert_int_equal(rd.overruns, 1);

	for (uint32_t i = 13; i < 20; i++)
		rd_expect(&rd, i);

	assert_int_equal(lfb_read(&rd, &f), 0);
	assert_int_equal(rd.overruns, 1);

	free(mem);
}

static void test_lag(void **state)
{
	void *mem;
	lfb_t *b = mk_buf(16, &mem);
	lfb_rd_t rd;
	tframe_t f;

	(void)state;

	lfb_seek(b, &rd, LFB_OLDEST);
	assert_int_equal(lfb_lag(&rd), 0);

	for (uint32_t i = 0; i < 5; i++)
		wr(b, i);

	assert_int_equal(lfb_lag(&rd), 5);

	rd_expect(&rd, 0);
	assert_int_equal(lfb_lag(&rd), 4);

	/* lap the reader: lag clamps to depth */
	for (uint32_t i = 5; i < 53; i++)
		wr(b, i);

	assert_int_equal(lfb_lag(&rd), 16);

	/* resync lands depth - LFB_CRUSH(16) = 15 back from w=53 */
	assert_int_equal(lfb_read(&rd, &f), -EPIPE);

	for (uint32_t i = 38; i < 53; i++)
		rd_expect(&rd, i);

	assert_int_equal(lfb_read(&rd, &f), 0);
	free(mem);
}

static void test_wslot(void **state)
{
	void *mem;
	lfb_t *b = mk_buf(8, &mem);
	lfb_rd_t rd;
	tframe_t *slot;

	(void)state;

	lfb_seek(b, &rd, LFB_OLDEST);

	slot = (tframe_t *)lfb_get_wslot(b);
	assert_ptr_equal(slot, lfb_data(b));

	slot->seq = 7;
	slot->val = 21;

	/* not visible until published */
	tframe_t f;
	assert_int_equal(lfb_read(&rd, &f), 0);

	lfb_commit_wslot(b);
	rd_expect(&rd, 7);

	/* next slot advanced by one frame */
	assert_ptr_equal(lfb_get_wslot(b), lfb_data(b) + TF_SZ);

	free(mem);
}

static void test_user_area(void **state)
{
	size_t sz = lfb_memsz(TF_SZ, 16, 24);
	void *mem = malloc(sz);
	lfb_t *b;
	uint8_t *u;
	lfb_rd_t rd;

	(void)state;
	assert_non_null(mem);
	memset(mem, 0xff, sz);

	b = lfb_init(mem, sz, TF_SZ, 16, 24);
	assert_non_null(b);

	u = (uint8_t *)lfb_user(b);
	assert_non_null(u);
	assert_ptr_equal(u, lfb_cuser(b));
	assert_int_equal((uintptr_t)u % _Alignof(max_align_t), 0);

	/* zeroed by lfb_prepare despite the dirty region */
	for (int i = 0; i < 24; i++)
		assert_int_equal(u[i], 0);

	/* payload starts aligned after the user area */
	assert_ptr_equal(lfb_data(b), u + LFB_ALIGN_UP(24));

	/* frames and user area don't interfere */
	lfb_seek(b, &rd, LFB_OLDEST);
	wr(b, 1);
	memset(u, 0xab, 24);
	rd_expect(&rd, 1);
	assert_int_equal(u[23], 0xab);

	free(mem);
}

static void test_no_user_area(void **state)
{
	void *mem;
	lfb_t *b = mk_buf(8, &mem);

	(void)state;
	assert_null(lfb_user(b));
	assert_null(lfb_cuser(b));
	free(mem);
}

static void test_prep_publish(void **state)
{
	size_t sz = lfb_memsz(TF_SZ, 16, 8);
	void *mem = malloc(sz);
	lfb_t *b;

	(void)state;
	assert_non_null(mem);
	memset(mem, 0xff, sz);

	b = lfb_prepare(mem, sz, TF_SZ, 16, 8);
	assert_non_null(b);

	/* not attachable until published */
	assert_null(lfb_attach(mem, sz));

	*(uint64_t *)lfb_user(b) = 0xdeadbeefcafeULL;
	lfb_publish(b);

	assert_ptr_equal(lfb_attach(mem, sz), b);
	assert_int_equal(*(const uint64_t *)lfb_cuser(b), 0xdeadbeefcafeULL);

	free(mem);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_memsz),
		cmocka_unit_test(test_init_invalid),
		cmocka_unit_test(test_attach),
		cmocka_unit_test(test_write_read),
		cmocka_unit_test(test_rslot_commit),
		cmocka_unit_test(test_rslot_no_commit),
		cmocka_unit_test(test_rslot_overrun_during_borrow),
		cmocka_unit_test(test_rslot_overrun_before_borrow),
		cmocka_unit_test(test_rslot_read_mixed),
		cmocka_unit_test(test_newest),
		cmocka_unit_test(test_wrap),
		cmocka_unit_test(test_overrun_resync),
		cmocka_unit_test(test_lag),
		cmocka_unit_test(test_wslot),
		cmocka_unit_test(test_user_area),
		cmocka_unit_test(test_no_user_area),
		cmocka_unit_test(test_prep_publish),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
