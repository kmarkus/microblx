#include <stdlib.h>
#include <errno.h>

#include "lfq.h"

int lfq_init(lfq_t *q, size_t capacity)
{
	int ret = -ENOMEM;

	q->head = 0;
	q->tail = 0;
	q->capacity = capacity;
	atomic_init(&q->count, 0);

	q->data = calloc(capacity, sizeof(void *));
	if (q->data == NULL)
		goto out;

	q->slot_state = calloc(capacity, sizeof(int));
	if (q->slot_state == NULL) {
		free(q->data);
		goto out;
	}

	/* init all slots as empty */
	for (size_t i = 0; i < capacity; ++i)
		q->slot_state[i] = 0;

	ret = 0;
out:
	return ret;
}

void lfq_free(lfq_t *q)
{
	free(q->slot_state);
	free(q->data);
}

int lfq_enqueue(lfq_t *q, void *element)
{
	size_t cur_tail, next_tail;

	while (true) {
		/* check if the queue is full */
		if (atomic_load(&q->count) == q->capacity)
			return -ENOSPC;

		cur_tail = atomic_load(&q->tail);
		next_tail = (cur_tail + 1) % q->capacity;

		/* try to move the tail forward */
		if (atomic_compare_exchange_weak(&q->tail, &cur_tail, next_tail)) {
			/* ensure slot is empty before writing */
			while (atomic_load(&q->slot_state[cur_tail]) != 0) {
				/* busy-wait until slot becomes empty */
			}

			/* store the data */
			q->data[cur_tail] = element;

			/* mark the slot as used */
			atomic_store(&q->slot_state[cur_tail], 1);

			/* increment the count */
			atomic_fetch_add(&q->count, 1);

			return 0;
		}
		/* retry if CAS failed due to contention */
	}
}

int lfq_dequeue(lfq_t *q, void **element)
{
	size_t cur_head, next_head;

	while (true) {
		/* check if the queue is empty */
		if (atomic_load(&q->count) == 0)
			return -ENODATA;

		cur_head = atomic_load(&q->head);
		next_head = (cur_head + 1) % q->capacity;

		/* try to move the head forward */
		if (atomic_compare_exchange_weak(&q->head, &cur_head, next_head)) {
			/* ensure the slot is used before reading */
			while (atomic_load(&q->slot_state[cur_head]) != 1) {
				/* busy-wait until slot becomes full */
			}

			/* read the data */
			*element = q->data[cur_head];

			/* mark the slot as empty */
			atomic_store(&q->slot_state[cur_head], 0);

			/* decrement the count */
			atomic_fetch_sub(&q->count, 1);

			return 0;
		}
		/* retry if CAS failed due to contention */
	}
}
