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

/* NULL is a legal element value (capacity 1 and >= 2) */
static void test_null_element(void **state)
{
	(void)state;

	lfq_t q;
	void *e = (void *)0xdeadbeef;

	/* capacity 1 (mailbox) */
	assert_int_equal(lfq_init(&q, 1), 0);
	assert_int_equal(lfq_enqueue(&q, NULL), 0);
	assert_int_equal(lfq_enqueue(&q, &e), -ENOSPC);
	assert_int_equal(lfq_dequeue(&q, &e), 0);
	assert_null(e);
	assert_int_equal(lfq_dequeue(&q, &e), -ENODATA);
	lfq_free(&q);

	/* capacity >= 2 */
	e = (void *)0xdeadbeef;
	assert_int_equal(lfq_init(&q, 2), 0);
	assert_int_equal(lfq_enqueue(&q, NULL), 0);
	assert_int_equal(lfq_dequeue(&q, &e), 0);
	assert_null(e);
	assert_int_equal(lfq_dequeue(&q, &e), -ENODATA);
	lfq_free(&q);
}

/*
 * MPMC stress test modeled on the lfrb usage pattern: a fixed pool of
 * NELEM elements shuttles between two queues of capacity NELEM.
 *
 * For capacity >= 2, enqueue may return a transient -ENOSPC even
 * though the destination is not logically full (a concurrent dequeue
 * may not yet have released its slot), so callers must retry. The
 * invariants checked are: all shuttle threads terminate (no
 * livelock, no permanently full queue) and no element is lost or
 * duplicated.
 *
 * For capacity == 1 (mailbox), full/empty answers are exact: the
 * enqueueing thread holds the only element, so the destination can
 * never be full and -ENOSPC must not occur at all.
 */
#define SHUTTLE_MAX_NELEM 4
#define SHUTTLE_ITERS	  100000
#define SHUTTLE_NTHREADS  2	/* per direction */

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

/* run the shuttle with the given pool/queue size, return total ENOSPC */
static unsigned long run_shuttle(int nelem)
{
	lfq_t q1, q2;
	pthread_t tids[2 * SHUTTLE_NTHREADS];
	struct shuttle_ctx ctx[2 * SHUTTLE_NTHREADS];
	int elem[SHUTTLE_MAX_NELEM] = { 0 };
	void *e;
	int cnt = 0;
	unsigned long enospc = 0;

	assert_true(nelem <= SHUTTLE_MAX_NELEM);
	assert_int_equal(lfq_init(&q1, nelem), 0);
	assert_int_equal(lfq_init(&q2, nelem), 0);

	for (int i = 0; i < nelem; i++)
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

	for (int i = 0; i < 2 * SHUTTLE_NTHREADS; i++)
		enospc += ctx[i].enospc;

	/* conservation: all elements accounted for, none lost or duped */
	while (lfq_dequeue(&q1, &e) == 0) {
		(*(int *)e)++;
		cnt++;
	}
	while (lfq_dequeue(&q2, &e) == 0) {
		(*(int *)e)++;
		cnt++;
	}

	assert_int_equal(cnt, nelem);

	for (int i = 0; i < nelem; i++)
		assert_int_equal(elem[i], 1);

	lfq_free(&q1);
	lfq_free(&q2);

	return enospc;
}

static void test_mpmc_conservation(void **state)
{
	(void)state;
	run_shuttle(SHUTTLE_MAX_NELEM);
}

static void test_mpmc_mailbox_exact(void **state)
{
	(void)state;
	assert_int_equal(run_shuttle(1), 0);
}

/*
 * SPSC FIFO ordering: with a single producer and a single consumer,
 * elements must be dequeued in exactly the order they were enqueued.
 */
#define ORDER_ITERS 100000

static void *order_producer(void *arg)
{
	lfq_t *q = arg;

	for (uintptr_t i = 1; i <= ORDER_ITERS;) {
		if (lfq_enqueue(q, (void *)i) == 0)
			i++;
	}
	return NULL;
}

static void run_spsc_order(size_t capacity)
{
	lfq_t q;
	pthread_t tid;
	void *e;

	assert_int_equal(lfq_init(&q, capacity), 0);
	assert_int_equal(pthread_create(&tid, NULL, order_producer, &q), 0);

	for (uintptr_t expect = 1; expect <= ORDER_ITERS;) {
		if (lfq_dequeue(&q, &e) != 0)
			continue;
		assert_int_equal((uintptr_t)e, expect);
		expect++;
	}

	assert_int_equal(pthread_join(tid, NULL), 0);
	assert_int_equal(lfq_dequeue(&q, &e), -ENODATA);
	lfq_free(&q);
}

static void test_spsc_order_cap1(void **state)
{
	(void)state;
	run_spsc_order(1);
}

static void test_spsc_order_cap4(void **state)
{
	(void)state;
	run_spsc_order(4);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_lfq_basic_enqueue_dequeue),
		cmocka_unit_test(test_lfq_multiple_enqueue_dequeue),
		cmocka_unit_test(test_capacity_one),
		cmocka_unit_test(test_null_element),
		cmocka_unit_test(test_mpmc_conservation),
		cmocka_unit_test(test_mpmc_mailbox_exact),
		cmocka_unit_test(test_spsc_order_cap1),
		cmocka_unit_test(test_spsc_order_cap4),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
