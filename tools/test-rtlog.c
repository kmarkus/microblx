/*
 * test-rtlog: rtlog shm ring buffer stress test
 *
 * Copyright (C) 2026 Markus Klotzbuecher <mk@mkio.de>
 *
 * SPDX-License-Identifier: MPL-2.0
 *
 * In the spirit of liblfq/test-lfq.c: multiple writer threads hammer
 * the shm log concurrently via __ubx_log while multiple independent
 * reader clients (librtlog_client) each verify that they receive
 * *every* message uncorrupted and in per-writer order.
 *
 * Unlike lfq, rtlog is a lossy broadcast ring: every reader sees all
 * messages, but a writer lapping a reader overruns it. To make "no
 * loss" verifiable, messages are written in rounds of no more than
 * LOG_BUFFER_DEPTH frames and all readers must fully drain a round
 * before the next one starts.
 *
 * Each message encodes writer id (in src and msg), a per-writer
 * sequence number, a level derived from the sequence and a fill
 * pattern. Readers check all of these and that per-writer sequences
 * increase gapless from 0.
 *
 * NOTE: this test unlinks and recreates the global log shm
 * (/dev/shm/rtlog.logshm); don't run it alongside a production ubx
 * application.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>
#include <getopt.h>
#include <time.h>
#include <sys/mman.h>

#include "ubx.h"
#include "rtlog_client.h"

#define FILL_LEN	32
#define ROUND_MARGIN	100	/* frames headroom to LOG_BUFFER_DEPTH */
#define STALL_TIMEOUT_S 10	/* fail if readers make no progress */

int num_writers = 8;
int num_readers = 4;
int num_msgs = 0;		/* per writer, per round; 0: auto */
int num_rounds = 50;

int verbose = 0;

#define inf(fmt, ...) if (verbose) printf(fmt, __VA_ARGS__);

ubx_node_t nd;			/* only .log/.log_data are used */
pthread_barrier_t round_start;	/* num_writers + 1 (main) */
atomic_int readers_ready;
atomic_ulong *consumed;		/* per reader */

unsigned long total_expected;	/* num_writers * num_msgs * num_rounds */

static void print_usage(const char *progname)
{
	fprintf(stderr,
		"Usage: %s [-w writers] [-r readers] [-o msgs] [-n rounds] [-v] [-h]\n"
		"  -w <int>    Number of writer threads (default: 8)\n"
		"  -r <int>    Number of reader threads (default: 4)\n"
		"  -o <int>    Messages per writer per round (default: auto)\n"
		"  -n <int>    Number of rounds (default: 50)\n"
		"  -v          Verbose output\n"
		"  -h          Show this help message and exit\n"
		"writers * msgs must stay below the buffer depth of %u\n",
		progname, LOG_BUFFER_DEPTH - ROUND_MARGIN);
}

static char fill_char(int wid, unsigned int seq)
{
	return 'A' + (wid * 7 + seq) % 26;
}

static void *writer_thread(void *arg)
{
	int id = *(int *)arg;
	char src[UBX_BLOCK_NAME_MAXLEN + 1];
	char fill[FILL_LEN + 1];
	unsigned int seq = 0;

	snprintf(src, sizeof(src), "w%03d", id);

	for (int r = 0; r < num_rounds; r++) {
		pthread_barrier_wait(&round_start);

		for (int i = 0; i < num_msgs; i++, seq++) {
			memset(fill, fill_char(id, seq), FILL_LEN);
			fill[FILL_LEN] = '\0';
			__ubx_log(seq % 8, &nd, src,
				  "w=%d s=%u f=%s", id, seq, fill);
		}
	}
	inf("writer %d finished.\n", id);
	return NULL;
}

/*
 * verify_frame - check a received frame for corruption and ordering
 *
 * @return 0 if OK, -1 on corruption
 */
static int verify_frame(int rid, const struct ubx_log_msg *m,
			unsigned int *next_seq)
{
	int wid, len;
	unsigned int seq;
	char fill[FILL_LEN + 1];
	char exp_src[UBX_BLOCK_NAME_MAXLEN + 1];

	if (sscanf(m->msg, "w=%d s=%u f=%32s%n", &wid, &seq, fill, &len) != 3) {
		fprintf(stderr, "reader %d: unparsable msg '%.*s'\n",
			rid, UBX_LOG_MSG_MAXLEN, m->msg);
		return -1;
	}

	if (wid < 0 || wid >= num_writers) {
		fprintf(stderr, "reader %d: invalid writer id %d\n", rid, wid);
		return -1;
	}

	snprintf(exp_src, sizeof(exp_src), "w%03d", wid);

	if (strcmp(m->src, exp_src) != 0) {
		fprintf(stderr, "reader %d: src '%s' != expected '%s'\n",
			rid, m->src, exp_src);
		return -1;
	}

	if (m->level != (int)(seq % 8)) {
		fprintf(stderr, "reader %d: w%d s=%u: level %d != %u\n",
			rid, wid, seq, m->level, seq % 8);
		return -1;
	}

	if (strlen(fill) != FILL_LEN) {
		fprintf(stderr, "reader %d: w%d s=%u: fill len %zu != %u\n",
			rid, wid, seq, strlen(fill), FILL_LEN);
		return -1;
	}

	for (int i = 0; i < FILL_LEN; i++) {
		if (fill[i] != fill_char(wid, seq)) {
			fprintf(stderr,
				"reader %d: w%d s=%u: corrupt fill '%s'\n",
				rid, wid, seq, fill);
			return -1;
		}
	}

	if (seq != next_seq[wid]) {
		fprintf(stderr,
			"reader %d: w%d: seq %u != expected %u (%s)\n",
			rid, wid, seq, next_seq[wid],
			seq > next_seq[wid] ? "lost msgs" : "dup/reorder");
		return -1;
	}

	next_seq[wid] = seq + 1;
	return 0;
}

static void *reader_thread(void *arg)
{
	int id = *(int *)arg;
	logc_info_t ci;
	struct ubx_log_msg m;
	volatile log_frame_t *frame;
	unsigned int *next_seq;
	unsigned long cnt = 0;

	next_seq = calloc(num_writers, sizeof(unsigned int));
	if (next_seq == NULL) {
		perror("failed to alloc next_seq");
		exit(1);
	}

	if (logc_init(&ci, LOG_SHM_FILENAME, sizeof(struct ubx_log_msg)) != 0) {
		fprintf(stderr, "reader %d: logc_init failed\n", id);
		exit(1);
	}

	logc_reset_read(&ci);
	atomic_fetch_add(&readers_ready, 1);

	while (cnt < total_expected) {
		switch (logc_read_frame(&ci, &frame)) {
		case NO_DATA:
			usleep(100);
			continue;

		case NEW_DATA:
			memcpy(&m, (const void *)frame, sizeof(m));

			if (verify_frame(id, &m, next_seq) != 0)
				exit(1);

			cnt++;
			atomic_store(&consumed[id], cnt);
			break;

		case OVERRUN:
			fprintf(stderr, "reader %d: OVERRUN after %lu msgs\n",
				id, cnt);
			exit(1);

		default:
			fprintf(stderr, "reader %d: read ERROR\n", id);
			exit(1);
		}
	}

	/* every writer's final sequence must have been reached */
	for (int w = 0; w < num_writers; w++) {
		if (next_seq[w] != (unsigned int)(num_rounds * num_msgs)) {
			fprintf(stderr, "reader %d: w%d: %u of %d msgs\n",
				id, w, next_seq[w], num_rounds * num_msgs);
			exit(1);
		}
	}

	logc_close(&ci);
	free(next_seq);
	inf("reader %d finished (%lu msgs).\n", id, cnt);
	return NULL;
}

/*
 * wait_consumed - wait until all readers have consumed cnt messages,
 * failing on stall
 */
static void wait_consumed(unsigned long cnt)
{
	int stall_ms = 0;
	unsigned long min, last_min = 0;

	while (1) {
		min = atomic_load(&consumed[0]);
		for (int i = 1; i < num_readers; i++) {
			unsigned long c = atomic_load(&consumed[i]);
			min = (c < min) ? c : min;
		}

		if (min >= cnt)
			return;

		if (min != last_min) {
			last_min = min;
			stall_ms = 0;
		} else if (++stall_ms > STALL_TIMEOUT_S * 1000) {
			fprintf(stderr,
				"stall: readers stuck at %lu of %lu msgs\n",
				min, cnt);
			exit(1);
		}
		usleep(1000);
	}
}

static double timespec_diff_sec(struct timespec *start, struct timespec *end)
{
	return (end->tv_sec - start->tv_sec) +
	       (end->tv_nsec - start->tv_nsec) * 1e-9;
}

int main(int argc, char *argv[])
{
	int opt;
	struct timespec t_start, t_end;

	while ((opt = getopt(argc, argv, "w:r:o:n:vh")) != -1) {
		switch (opt) {
		case 'w':
			num_writers = atoi(optarg);
			break;
		case 'r':
			num_readers = atoi(optarg);
			break;
		case 'o':
			num_msgs = atoi(optarg);
			break;
		case 'n':
			num_rounds = atoi(optarg);
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

	if (num_writers < 1 || num_readers < 1 ||
	    num_msgs < 0 || num_rounds < 1) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/*
	 * a round (writers * msgs) must fit into the ring: rtlog
	 * writers never block but overrun the slowest reader, so "no
	 * loss" is only verifiable if no reader can be lapped before
	 * draining the round. By default use the largest valid round.
	 */
	if (num_msgs == 0)
		num_msgs = (LOG_BUFFER_DEPTH - ROUND_MARGIN) / num_writers;

	if (num_msgs < 1) {
		fprintf(stderr, "too many writers (max %u)\n",
			LOG_BUFFER_DEPTH - ROUND_MARGIN);
		exit(EXIT_FAILURE);
	}

	if (num_writers * num_msgs > LOG_BUFFER_DEPTH - ROUND_MARGIN) {
		fprintf(stderr,
			"writers * msgs (%d) exceeds max round size %u\n",
			num_writers * num_msgs, LOG_BUFFER_DEPTH - ROUND_MARGIN);
		exit(EXIT_FAILURE);
	}

	total_expected = (unsigned long)num_writers * num_msgs * num_rounds;

	inf("writers=%d, readers=%d, msgs/round=%d, rounds=%d, total=%lu\n",
	    num_writers, num_readers, num_msgs, num_rounds, total_expected);

	/* start with a fresh shm so the write offset is at zero */
	shm_unlink(LOG_SHM_FILENAME);

	memset(&nd, 0, sizeof(nd));

	if (ubx_log_init(&nd) != 0) {
		fprintf(stderr, "ubx_log_init failed\n");
		exit(1);
	}

	consumed = calloc(num_readers, sizeof(atomic_ulong));
	pthread_t *writers = malloc(num_writers * sizeof(pthread_t));
	pthread_t *readers = malloc(num_readers * sizeof(pthread_t));
	int *writer_ids = malloc(num_writers * sizeof(int));
	int *reader_ids = malloc(num_readers * sizeof(int));

	if (!consumed || !writers || !readers || !writer_ids || !reader_ids) {
		perror("failed to allocate thread metadata");
		exit(1);
	}

	pthread_barrier_init(&round_start, NULL, num_writers + 1);

	for (int i = 0; i < num_readers; i++) {
		reader_ids[i] = i;
		if (pthread_create(&readers[i], NULL, reader_thread,
				   &reader_ids[i]) != 0) {
			perror("failed to create reader thread");
			exit(EXIT_FAILURE);
		}
	}

	/* all readers must be attached before the first msg is written */
	while (atomic_load(&readers_ready) < num_readers)
		usleep(1000);

	for (int i = 0; i < num_writers; i++) {
		writer_ids[i] = i;
		if (pthread_create(&writers[i], NULL, writer_thread,
				   &writer_ids[i]) != 0) {
			perror("failed to create writer thread");
			exit(EXIT_FAILURE);
		}
	}

	clock_gettime(CLOCK_MONOTONIC, &t_start);

	for (int r = 0; r < num_rounds; r++) {
		pthread_barrier_wait(&round_start);
		wait_consumed((unsigned long)(r + 1) * num_writers * num_msgs);
		inf("round %d/%d done\n", r + 1, num_rounds);
	}

	for (int i = 0; i < num_writers; i++)
		pthread_join(writers[i], NULL);

	for (int i = 0; i < num_readers; i++)
		pthread_join(readers[i], NULL);

	clock_gettime(CLOCK_MONOTONIC, &t_end);

	double elapsed = timespec_diff_sec(&t_start, &t_end);
	printf("elapsed: %.3f s, %.0f msgs/s written, %.0f msgs/s verified\n",
	       elapsed, total_expected / elapsed,
	       total_expected * num_readers / elapsed);

	/* reaching this point means every reader verified every msg */
	fprintf(stderr, "test_rtlog: OK\n");

	ubx_log_cleanup(&nd);
	pthread_barrier_destroy(&round_start);
	free(consumed);
	free(writers);
	free(readers);
	free(writer_ids);
	free(reader_ids);

	exit(0);
}
