#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "lfq.h"

#define NUM_WRITERS 30
#define NUM_READERS 30
#define CAPACITY 100
#define NUM_OPERATIONS 10000

lfq_t queue;

void *writer_thread(void *arg)
{
	int id = *(int *)arg;
	for (int i = 0; i < NUM_OPERATIONS; ++i) {
		int *element = malloc(sizeof(int));
		*element = id * 1000 + i; /* unique value for each writer */
		while (lfq_enqueue(&queue, element)) {
			printf("writer %d contention enqueuing %d\n", id, *element);
			usleep(1);
		}
		printf("writer %d: enqueued %d\n", id, *element);
	}
	return NULL;
}

void *reader_thread(void *arg)
{
	int id = *(int *)arg;
	for (int i = 0; i < NUM_OPERATIONS; ++i) {
		void *element;
		while (lfq_dequeue(&queue, &element)) {
			printf("reader %d contention dequeuing\n", id);
			usleep(1);
		}
		printf("reader %d: dequeued %d\n", id, *(int *)element);
		free(element);
	}
	return NULL;
}

int main()
{
	lfq_init(&queue, CAPACITY);

	pthread_t writers[NUM_WRITERS];
	pthread_t readers[NUM_READERS];
	int writer_ids[NUM_WRITERS];
	int reader_ids[NUM_READERS];

	/* Create writer threads */
	for (int i = 0; i < NUM_WRITERS; ++i) {
		writer_ids[i] = i;
		if (pthread_create(&writers[i], NULL, writer_thread, &writer_ids[i]) != 0) {
			perror("failed to create writer thread");
			return 1;
		}
	}

	/* Create reader threads */
	for (int i = 0; i < NUM_READERS; ++i) {
		reader_ids[i] = i;
		if (pthread_create(&readers[i], NULL, reader_thread, &reader_ids[i]) != 0) {
			perror("failed to create reader thread");
			return 1;
		}
	}

	/* Wait for all threads to complete */
	for (int i = 0; i < NUM_WRITERS; ++i)
		pthread_join(writers[i], NULL);

	for (int i = 0; i < NUM_READERS; ++i)
		pthread_join(readers[i], NULL);

	lfq_free(&queue);

	return 0;
}
