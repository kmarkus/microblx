#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <time.h>
#include <liblfds611.h>

#ifdef DEBUG
#define dbg(fmt, ...)                                   \
	(fprintf(stderr, "%s:%u ", __func__, __LINE__), \
	 fprintf(stderr, fmt, __VA_ARGS__), fprintf(stderr, "\n"))
#else
#define dbg(fmt, ...) \
	do {          \
	} while (0)
#endif

struct lfds611_queue_state *queue;
atomic_bool *elem_written;
atomic_bool *elem_read;
int *elements;

int num_threads = 30;
int queue_capacity = 4;
int num_operations = 100000;
int total_elements;

int verbose = 0;

#define inf(fmt, ...) if (verbose) printf(fmt, __VA_ARGS__);

static void print_usage(const char *progname)
{
	fprintf(stderr,
		"Usage: %s [-c capacity] [-t threads] [-o operations] [-h]\n"
		"  -c <int>    Queue capacity (default: 4)\n"
		"  -t <int>    Number of reader/writer threads (default: 30)\n"
		"  -o <int>    Number of operations per thread (default: 100000)\n"
		"  -v          Verbose output\n"
		"  -h          Show this help message and exit\n",
		progname);
}

static void *writer_thread(void *arg)
{
	int id = *(int *)arg;

	/* required for all threads that did not call lfds611_queue_new */
	lfds611_queue_use(queue);

	for (int i = 0; i < num_operations; ++i) {
		int key = id * num_operations + i;
		int *element = &elements[key];
		*element = key;

		while (lfds611_queue_enqueue(queue, element) == 0) {
			dbg("writer %d: queue full, retrying %d", id, *element);
			usleep(1);
		}

		if (atomic_exchange(&elem_written[key], true)) {
			fprintf(stderr,
				"Error: duplicate enqueue %d by writer %d\n",
				key, id);
			exit(1);
		}

		dbg("writer %d: enqueued %d", id, key);
	}
	inf("writer %d finished.\n", id);
	return NULL;
}

static void *reader_thread(void *arg)
{
	int id = *(int *)arg;

	lfds611_queue_use(queue);

	for (int i = 0; i < num_operations; ++i) {
		void *element;

		while (lfds611_queue_dequeue(queue, &element) == 0) {
			dbg("reader %d: queue empty", id);
			usleep(1);
		}

		int key = *(int *)element;

		if (atomic_exchange(&elem_read[key], true)) {
			fprintf(stderr,
				"Error: duplicate read %d by reader %d\n", key,
				id);
			exit(2);
		}

		dbg("reader %d: dequeued %d", id, key);
	}
	inf("reader %d finished.\n", id);
	return NULL;
}

static double timespec_diff_sec(struct timespec *start, struct timespec *end)
{
	return (end->tv_sec - start->tv_sec) +
	       (end->tv_nsec - start->tv_nsec) * 1e-9;
}

int main(int argc, char *argv[])
{
	int opt;
	int failed = 0;
	struct timespec t_start, t_end;

	while ((opt = getopt(argc, argv, "c:t:o:vh")) != -1) {
		switch (opt) {
		case 'c':
			queue_capacity = atoi(optarg);
			break;
		case 't':
			num_threads = atoi(optarg);
			break;
		case 'o':
			num_operations = atoi(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		case 'h':
			print_usage(argv[0]);
			exit(0);
		default:
			print_usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	total_elements = num_threads * num_operations;

	inf("Using capacity=%d, threads=%d, operations=%d\n",
	    queue_capacity, num_threads, num_operations);

	elements = malloc(total_elements * sizeof(int));
	elem_written = calloc(total_elements, sizeof(atomic_bool));
	elem_read = calloc(total_elements, sizeof(atomic_bool));

	if (!elements || !elem_written || !elem_read) {
		perror("Failed to allocate arrays");
		return 1;
	}

	if (lfds611_queue_new(&queue, queue_capacity) == 0) {
		fprintf(stderr, "lfds611_queue_new failed\n");
		return 1;
	}

	pthread_t *writers = malloc(num_threads * sizeof(pthread_t));
	pthread_t *readers = malloc(num_threads * sizeof(pthread_t));
	int *writer_ids = malloc(num_threads * sizeof(int));
	int *reader_ids = malloc(num_threads * sizeof(int));

	if (!writers || !readers || !writer_ids || !reader_ids) {
		perror("Failed to allocate thread metadata");
		return 1;
	}

	clock_gettime(CLOCK_MONOTONIC, &t_start);

	for (int i = 0; i < num_threads; ++i) {
		writer_ids[i] = i;
		if (pthread_create(&writers[i], NULL, writer_thread,
				   &writer_ids[i]) != 0) {
			perror("failed to create writer thread");
			exit(EXIT_FAILURE);
		}
	}

	for (int i = 0; i < num_threads; ++i) {
		reader_ids[i] = i;
		if (pthread_create(&readers[i], NULL, reader_thread,
				   &reader_ids[i]) != 0) {
			perror("failed to create reader thread");
			exit(EXIT_FAILURE);
		}
	}

	for (int i = 0; i < num_threads; ++i)
		pthread_join(writers[i], NULL);

	for (int i = 0; i < num_threads; ++i)
		pthread_join(readers[i], NULL);

	clock_gettime(CLOCK_MONOTONIC, &t_end);

	double elapsed = timespec_diff_sec(&t_start, &t_end);
	printf("elapsed: %.3f s, throughput: %.0f ops/s\n",
	       elapsed, total_elements / elapsed);

	for (int i = 0; i < total_elements; ++i) {
		if (atomic_load(&elem_written[i]) &&
		    !atomic_load(&elem_read[i])) {
			fprintf(stderr,
				"Error: element %d was written but not read\n",
				i);
			failed = 1;
		}
		if (!atomic_load(&elem_written[i]) &&
		    atomic_load(&elem_read[i])) {
			fprintf(stderr,
				"Error: element %d was read but not written\n",
				i);
			failed = 1;
		}
	}

	fprintf(stderr, "test_lfds: %s\n", failed == 0 ? "OK" : "FAILED");

	free(elements);
	free(elem_written);
	free(elem_read);
	free(writers);
	free(readers);
	free(writer_ids);
	free(reader_ids);
	lfds611_queue_delete(queue, NULL, NULL);

	exit(failed);
}
