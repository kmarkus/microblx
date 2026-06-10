#ifndef _LFQ_H_
#define _LFQ_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <errno.h>

typedef struct {
    size_t capacity;
    struct lfq_slot *slots;
    _Atomic uint64_t head;
    _Atomic uint64_t tail;
} lfq_t;


/**
 * @brief Initialize a lock-free queue
 * @param q pointer to the queue
 * @param capacity capacity of the queue (>=1, performs better if it is a power of 2)
 * @return 0 on success, negative errno on failure
 */
int lfq_init(lfq_t *q, size_t capacity);

/**
 * @brief Cleanup a lock-free queue.
 * @param q pointer to the queue
 */
void lfq_free(lfq_t *q);

/**
 * @brief Enqueue an element into the lock-free queue.
 *
 * Note that for capacity >= 2, -ENOSPC can be transient: a
 * concurrent dequeue may not yet have released its slot. Callers
 * that know the queue cannot be logically full must retry. For
 * capacity == 1 (single-slot mailbox) full/empty answers are exact.
 *
 * @param q pointer to the queue
 * @param pointer to element to enqueue
 * @return 0 on success, -ENOSPC if the queue is full.
 */
int lfq_enqueue(lfq_t *q, void *element);

/**
 * @brief Dequeue an element from the lock-free queue.
 *
 * Likewise, for capacity >= 2, -ENODATA can be transient if a
 * concurrent enqueue has not yet completed.
 *
 * @param q pointer to the queue
 * @param pointer[out] pointer to element pointer for storing dequeued element
 * @return 0 on success, -ENODATA if the queue is empty.
 */
int lfq_dequeue(lfq_t *q, void **element);

#endif /* _LFQ_H_ */
