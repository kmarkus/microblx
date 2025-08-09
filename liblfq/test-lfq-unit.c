#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <cmocka.h>
#include <errno.h>

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

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_lfq_basic_enqueue_dequeue),
		cmocka_unit_test(test_lfq_multiple_enqueue_dequeue),
		cmocka_unit_test(test_capacity_one),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
