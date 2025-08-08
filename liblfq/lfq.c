/* This is a simple implementation of Vyukov's algorithm adapted to
 * support capacity == 1.
 *
 * Copyright (C) 2025 Markus Klotzbuecher <mk@mkio.de>*
 * SPDX-License-Identifier: MPL-2.0
 */

#include "lfq.h"
#include <stdlib.h>
#include <string.h>

typedef struct lfq_slot {
	_Atomic uint64_t seq;
	_Atomic(void *) data;
} lfq_slot_t;

int lfq_init(lfq_t *q, size_t capacity)
{
	if (!q || capacity < 1)
		return -EINVAL;

	q->logical_capacity = capacity;
	q->capacity = capacity == 1 ? 2 : capacity;

	q->slots = calloc(q->capacity, sizeof(lfq_slot_t));
	if (!q->slots)
		return -ENOMEM;

	for (size_t i = 0; i < q->capacity; ++i)
		atomic_store(&q->slots[i].seq, i);

	atomic_store(&q->head, 0);
	atomic_store(&q->tail, 0);
	return 0;
}

void lfq_free(lfq_t *q)
{
	if (q && q->slots) {
		free(q->slots);
		q->slots = NULL;
		q->logical_capacity = 0;
		q->capacity = 0;
	}
}

int lfq_enqueue(lfq_t *q, void *element)
{
	const size_t capacity = q->capacity;
	lfq_slot_t *slots = q->slots;

	while (1) {
		uint64_t head = atomic_load_explicit(&q->head, memory_order_relaxed);
		uint64_t tail = atomic_load_explicit(&q->tail, memory_order_relaxed);

		/* this check is only necessary because we want to
		 * support queue size 1 */
		if (head - tail >= q->logical_capacity)
			return -ENOSPC;

		lfq_slot_t *slot = &slots[head % capacity];
		uint64_t seq = atomic_load_explicit(&slot->seq, memory_order_acquire);
		intptr_t diff = (intptr_t)seq - (intptr_t)head;

		if (diff == 0) {
			if (atomic_compare_exchange_weak_explicit(
				    &q->head, &head, head + 1,
				    memory_order_relaxed,
				    memory_order_relaxed)) {
				atomic_store_explicit(&slot->data, element, memory_order_relaxed);
				atomic_store_explicit(&slot->seq, head + 1, memory_order_release);
				return 0;
			}
		} else if (diff < 0) {
			return -ENOSPC;
		}
	}
}

int lfq_dequeue(lfq_t *q, void **element)
{
	const size_t capacity = q->capacity;
	lfq_slot_t *slots = q->slots;

	while (1) {
		uint64_t tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
		lfq_slot_t *slot = &slots[tail % capacity];
		uint64_t seq = atomic_load_explicit(&slot->seq, memory_order_acquire);
		intptr_t diff = (intptr_t)seq - (intptr_t)(tail + 1);

		if (diff == 0) {
			if (atomic_compare_exchange_weak_explicit(
				    &q->tail, &tail, tail + 1,
				    memory_order_relaxed,
				    memory_order_relaxed)) {
				*element = atomic_load_explicit(&slot->data, memory_order_relaxed);
				atomic_store_explicit(&slot->seq, tail + capacity, memory_order_release);
				return 0;
			}
		} else if (diff < 0) {
			return -ENODATA;
		}
	}
}
