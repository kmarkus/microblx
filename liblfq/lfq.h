#ifndef LOCKFREEQUEUE_H
#define LOCKFREEQUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>

typedef struct {
	_Atomic size_t count;
	_Atomic size_t head;
	_Atomic size_t tail;

	 size_t capacity;

	_Atomic int *slot_state; /* 0: empty, 1: full */
	void **data;
} lfq_t;

/**
 * @brief initialize a lock-free queue.
 *
 * @param q pointer to the lfq_t instance.
 */
int lfq_init(lfq_t *q, size_t capacity);

/**
 * @brief cleanup a lock-free queue.
 *
 * @param q pointer to the lfq_t instance.
 */

void lfq_free(lfq_t *q);

/**
 * @brief enqueue an element into the lock-free queue.
 *
 * @param q pointer to the LockFreeQueue instance.
 * @param element Pointer to the element to be added.
 * @return 0 if the element was successfully added, -ENOSPC if the queue is full.
 */
int lfq_enqueue(lfq_t *q, void *element);

/**
 * @brief Dequeue an element from the lock-free queue.
 *
 * @param q Pointer to the LockFreeQueue instance.
 * @param element Pointer to the location where the dequeued element will be stored.
 * @return 0 if an element was successfully dequeued, -ENODATA if the queue is empty.
 */
int lfq_dequeue(lfq_t *q, void **element);

#endif // LOCKFREEQUEUE_H
