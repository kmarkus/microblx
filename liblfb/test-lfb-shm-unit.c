/*
 * test-lfb-shm-unit.c - cmocka unit tests for the lfb shm layer
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lfb_shm.h"

typedef struct {
	uint32_t seq;
	uint32_t val;
} tframe_t;

#define TF_SZ ((uint32_t)sizeof(tframe_t))

/* per-run unique segment name to not collide with concurrent runs */
static char g_name[64];

static const char *segname(const char *suffix)
{
	snprintf(g_name, sizeof(g_name), "lfb-ut-%d-%s",
		 (int)getpid(), suffix);
	return g_name;
}

static void test_open_noent(void **state)
{
	lfb_shm_t s;

	(void)state;
	assert_int_equal(lfb_shm_open(&s, segname("noent")), -ENOENT);
}

static void test_create_invalid(void **state)
{
	lfb_shm_t s;
	char longname[LFB_SHM_NAME_MAX + 2];

	(void)state;

	assert_int_equal(lfb_shm_create(NULL, segname("inv"), TF_SZ, 8, 0),
			 -EINVAL);
	assert_int_equal(lfb_shm_create(&s, segname("inv"), 0, 8, 0), -EINVAL);
	assert_int_equal(lfb_shm_create(&s, segname("inv"), TF_SZ, 1, 0),
			 -EINVAL);

	memset(longname, 'x', sizeof(longname) - 1);
	longname[sizeof(longname) - 1] = '\0';
	assert_int_equal(lfb_shm_create(&s, longname, TF_SZ, 8, 0),
			 -ENAMETOOLONG);
}

static void test_create_open_rw(void **state)
{
	lfb_shm_t sp, sc;
	lfb_rd_t rd;
	tframe_t f;
	const char *name = segname("rw");

	(void)state;

	assert_int_equal(lfb_shm_create(&sp, name, TF_SZ, 64, 0), 0);
	assert_non_null(lfb_shm_lfb(&sp));

	/* consumer gets its own read-only mapping */
	assert_int_equal(lfb_shm_open(&sc, name), 0);
	assert_non_null(lfb_shm_lfb(&sc));
	assert_ptr_not_equal(lfb_shm_lfb(&sc), lfb_shm_lfb(&sp));
	assert_int_equal(lfb_shm_lfb(&sc)->depth, 64);
	assert_int_equal(lfb_shm_lfb(&sc)->frame_size, TF_SZ);

	lfb_seek(lfb_shm_lfb(&sc), &rd, LFB_OLDEST);

	for (uint32_t i = 0; i < 10; i++) {
		tframe_t w = { .seq = i, .val = i * 3 };
		lfb_write(lfb_shm_lfb(&sp), &w);
	}

	assert_int_equal(lfb_lag(&rd), 10);

	for (uint32_t i = 0; i < 10; i++) {
		assert_int_equal(lfb_read(&rd, &f), 1);
		assert_int_equal(f.seq, i);
		assert_int_equal(f.val, i * 3);
	}

	assert_int_equal(lfb_read(&rd, &f), 0);
	assert_int_equal(lfb_shm_stale(&sc), 0);

	lfb_shm_close(&sc);
	lfb_shm_destroy(&sp);

	assert_int_equal(lfb_shm_open(&sc, name), -ENOENT);
}

static void test_stale(void **state)
{
	lfb_shm_t sp, sp2, sc;
	const char *name = segname("stale");

	(void)state;

	assert_int_equal(lfb_shm_create(&sp, name, TF_SZ, 64, 0), 0);
	assert_int_equal(lfb_shm_open(&sc, name), 0);
	assert_int_equal(lfb_shm_stale(&sc), 0);

	/* producer gone -> stale */
	lfb_shm_destroy(&sp);
	assert_int_equal(lfb_shm_stale(&sc), 1);

	/* recreated (new inode, new geometry) -> still stale */
	assert_int_equal(lfb_shm_create(&sp2, name, TF_SZ, 128, 0), 0);
	assert_int_equal(lfb_shm_stale(&sc), 1);

	/* old mapping remains usable until closed */
	assert_int_equal(lfb_shm_lfb(&sc)->depth, 64);

	/* reopen picks up the new instance */
	lfb_shm_close(&sc);
	assert_int_equal(lfb_shm_open(&sc, name), 0);
	assert_int_equal(lfb_shm_stale(&sc), 0);
	assert_int_equal(lfb_shm_lfb(&sc)->depth, 128);

	lfb_shm_close(&sc);
	lfb_shm_destroy(&sp2);
}

/* create a raw segment of the given size, optionally poking a fake
 * header into it, and return the open() result on it */
static int open_raw(const char *name, size_t sz, int fake_header)
{
	lfb_shm_t s;
	int fd, ret;

	fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
	assert_true(fd >= 0);
	assert_int_equal(ftruncate(fd, (off_t)sz), 0);

	if (fake_header) {
		lfb_t *b = mmap(NULL, sz, PROT_READ | PROT_WRITE,
				MAP_SHARED, fd, 0);
		assert_ptr_not_equal(b, MAP_FAILED);
		b->frame_size = TF_SZ;
		b->depth = 64;
		b->user_sz = 0;
		atomic_store_explicit(&b->magic, 0xdeadbeef,
				      memory_order_release);
		munmap(b, sz);
	}

	close(fd);
	ret = lfb_shm_open(&s, name);

	if (ret == 0)
		lfb_shm_close(&s);

	shm_unlink(name);
	return ret;
}

static void test_open_eagain(void **state)
{
	size_t sz = lfb_memsz(TF_SZ, 64, 0);

	(void)state;

	/* full-size but zeroed (magic 0): producer mid-init */
	assert_int_equal(open_raw(segname("eagain1"), sz, 0), -EAGAIN);

	/* not even header-sized yet: producer mid-create */
	assert_int_equal(open_raw(segname("eagain2"), 4, 0), -EAGAIN);
}

static void test_open_eproto(void **state)
{
	size_t sz = lfb_memsz(TF_SZ, 64, 0);

	(void)state;

	/* nonzero but mismatching magic: foreign contents */
	assert_int_equal(open_raw(segname("eproto"), sz, 1), -EPROTO);
}

static void test_join(void **state)
{
	lfb_shm_t sj1, sj2, sjx, sc;
	lfb_rd_t rd;
	tframe_t f, w;
	const char *name = segname("join");

	(void)state;

	/* no segment yet: join creates it, unpublished */
	assert_int_equal(lfb_shm_join(&sj1, name, TF_SZ, 64,
				      sizeof(uint32_t)), 1);
	assert_non_null(lfb_shm_lfb(&sj1));

	/* consumers and other joiners must wait until published */
	assert_int_equal(lfb_shm_open(&sc, name), -EAGAIN);
	assert_int_equal(lfb_shm_join(&sj2, name, TF_SZ, 64,
				      sizeof(uint32_t)), -EAGAIN);

	/* creator initializes the user area, then publishes */
	*(uint32_t *)lfb_user(lfb_shm_lfb(&sj1)) = 42;
	lfb_shm_publish(&sj1);

	/* second party joins the existing segment and sees the user area */
	assert_int_equal(lfb_shm_join(&sj2, name, TF_SZ, 64,
				      sizeof(uint32_t)), 0);
	assert_int_equal(*(uint32_t *)lfb_user(lfb_shm_lfb(&sj2)), 42);

	/* joining with mismatching geometry is rejected */
	assert_int_equal(lfb_shm_join(&sjx, name, TF_SZ, 128,
				      sizeof(uint32_t)), -EPROTO);
	assert_int_equal(lfb_shm_join(&sjx, name, TF_SZ, 64, 0), -EPROTO);

	/* both parties write (serialized), a consumer sees all frames */
	assert_int_equal(lfb_shm_open(&sc, name), 0);
	lfb_seek(lfb_shm_lfb(&sc), &rd, LFB_OLDEST);
	assert_int_equal(*(const uint32_t *)lfb_cuser(lfb_shm_lfb(&sc)), 42);

	w = (tframe_t){ .seq = 0, .val = 0 };
	lfb_write(lfb_shm_lfb(&sj1), &w);
	w = (tframe_t){ .seq = 1, .val = 3 };
	lfb_write(lfb_shm_lfb(&sj2), &w);

	for (uint32_t i = 0; i < 2; i++) {
		assert_int_equal(lfb_read(&rd, &f), 1);
		assert_int_equal(f.seq, i);
		assert_int_equal(f.val, i * 3);
	}

	/* leaving joiners don't take the segment away */
	lfb_shm_close(&sj2);
	assert_int_equal(lfb_shm_stale(&sc), 0);

	lfb_shm_close(&sc);
	lfb_shm_destroy(&sj1);
	assert_int_equal(lfb_shm_open(&sc, name), -ENOENT);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_open_noent),
		cmocka_unit_test(test_create_invalid),
		cmocka_unit_test(test_create_open_rw),
		cmocka_unit_test(test_stale),
		cmocka_unit_test(test_open_eagain),
		cmocka_unit_test(test_open_eproto),
		cmocka_unit_test(test_join),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
