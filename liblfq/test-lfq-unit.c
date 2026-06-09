#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <cmocka.h>
#include <errno.h>
#include <pthread.h>

#include "lfq.h"

static void test_capacity_one(void **state)
{
	(void)state;

	lfq_t q;
	assert_int_equal(lfq_init(&q, 1), 0);

	int val = 42;
	int val2 = 99;
	void *ptr;

	/* First enqueue should succeed */
	assert_int_equal(lfq_enqueue(&q, &val), 0);

	/* Second should fail (queue full) */
	assert_int_equal(lfq_enqueue(&q, &val2), -ENOSPC);

	/* Dequeue the one value */
	assert_int_equal(lfq_dequeue(&q, &ptr), 0);
	assert_int_equal(*(int *)ptr, val);

	/* Now dequeue should fail (empty) */
	assert_int_equal(lfq_dequeue(&q, &ptr), -ENODATA);

	lfq_free(&q);
}

static void test_lfq_basic_enqueue_dequeue(void **state)
{
	(void)state;

	lfq_t q;
	int ret = lfq_init(&q, 2);
	assert_int_equal(ret, 0);

	void *element = NULL;

	/* Queue should accept one element */
	int value1 = 42;
	ret = lfq_enqueue(&q, &value1);
	assert_int_equal(ret, 0);

	/* Second enqueue should succeed with -ENOSPC */
	int value2 = 999;
	ret = lfq_enqueue(&q, &value2);
	assert_int_equal(ret, 0);

	/* third enqueue should fail with -ENOSPC */
	int value3 = 7777;
	ret = lfq_enqueue(&q, &value3);
	assert_int_equal(ret, -ENOSPC);

	/* Dequeue the elements */
	ret = lfq_dequeue(&q, &element);
	assert_int_equal(ret, 0);
	assert_non_null(element);
	assert_int_equal(*(int *)element, value1);

	/* Dequeue the elements */
	ret = lfq_dequeue(&q, &element);
	assert_int_equal(ret, 0);
	assert_non_null(element);
	assert_int_equal(*(int *)element, value2);

	/* Now queue is empty again */
	ret = lfq_dequeue(&q, &element);
	assert_int_equal(ret, -ENODATA);

	lfq_free(&q);
}

static void test_lfq_multiple_enqueue_dequeue(void **state)
{
	(void)state;

	lfq_t q;
	int ret = lfq_init(&q, 3);
	assert_int_equal(ret, 0);

	int a = 10, b = 20, c = 30, d = 99;
	void *element = NULL;

	ret = lfq_enqueue(&q, &a);
	assert_int_equal(ret, 0);

	ret = lfq_enqueue(&q, &b);
	assert_int_equal(ret, 0);

	ret = lfq_enqueue(&q, &c);
	assert_int_equal(ret, 0);

	ret = lfq_enqueue(&q, &d);
	assert_int_equal(ret, -ENOSPC);

	ret = lfq_dequeue(&q, &element);
	assert_int_equal(ret, 0);
	assert_int_equal(*(int *)element, a);

	ret = lfq_dequeue(&q, &element);
	assert_int_equal(ret, 0);
	assert_int_equal(*(int *)element, b);

	ret = lfq_dequeue(&q, &element);
	assert_int_equal(ret, 0);
	assert_int_equal(*(int *)element, c);

	/* Queue is empty now */
	ret = lfq_dequeue(&q, &element);
	assert_int_equal(ret, -ENODATA);

	lfq_free(&q);
}

/*
 * MPMC stress test modeled on the lfrb usage pattern: a fixed pool of
 * elements shuttles between two queues.
 *
 * Note that enqueue may return a transient -ENOSPC even though the
 * destination is not logically full (a concurrent dequeue may not
 * yet have released its slot), so callers must retry. This test
 * checks the actual invariants: all shuttle threads terminate (no
 * livelock, no permanently full queue) and no element is lost or
 * duplicated.
 */
#define SHUTTLE_NELEM	 4
#define SHUTTLE_ITERS	 100000
#define SHUTTLE_NTHREADS 2	/* per direction */

struct shuttle_ctx {
	lfq_t *src;
	lfq_t *dst;
	unsigned long enospc;
};

static void *shuttle(void *arg)
{
	struct shuttle_ctx *c = arg;
	void *e;

	for (int i = 0; i < SHUTTLE_ITERS;) {
		if (lfq_dequeue(c->src, &e) != 0)
			continue;

		while (lfq_enqueue(c->dst, e) != 0)
			c->enospc++;

		i++;
	}
	return NULL;
}

static void test_mpmc_no_spurious_enospc(void **state)
{
	(void)state;

	lfq_t q1, q2;
	pthread_t tids[2 * SHUTTLE_NTHREADS];
	struct shuttle_ctx ctx[2 * SHUTTLE_NTHREADS];
	int elem[SHUTTLE_NELEM] = { 0 };
	void *e;
	int cnt = 0;

	assert_int_equal(lfq_init(&q1, SHUTTLE_NELEM), 0);
	assert_int_equal(lfq_init(&q2, SHUTTLE_NELEM), 0);

	for (int i = 0; i < SHUTTLE_NELEM; i++)
		assert_int_equal(lfq_enqueue(&q1, &elem[i]), 0);

	for (int i = 0; i < 2 * SHUTTLE_NTHREADS; i++) {
		ctx[i].src = (i % 2) ? &q1 : &q2;
		ctx[i].dst = (i % 2) ? &q2 : &q1;
		ctx[i].enospc = 0;
		assert_int_equal(
			pthread_create(&tids[i], NULL, shuttle, &ctx[i]), 0);
	}

	for (int i = 0; i < 2 * SHUTTLE_NTHREADS; i++)
		assert_int_equal(pthread_join(tids[i], NULL), 0);

	/* conservation: all elements accounted for, none lost or duped */
	while (lfq_dequeue(&q1, &e) == 0) {
		(*(int *)e)++;
		cnt++;
	}
	while (lfq_dequeue(&q2, &e) == 0) {
		(*(int *)e)++;
		cnt++;
	}

	assert_int_equal(cnt, SHUTTLE_NELEM);

	for (int i = 0; i < SHUTTLE_NELEM; i++)
		assert_int_equal(elem[i], 1);

	lfq_free(&q1);
	lfq_free(&q2);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_lfq_basic_enqueue_dequeue),
		cmocka_unit_test(test_lfq_multiple_enqueue_dequeue),
		cmocka_unit_test(test_capacity_one),
		cmocka_unit_test(test_mpmc_no_spurious_enospc),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
