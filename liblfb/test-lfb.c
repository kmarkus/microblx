/*
 * test-lfb.c - lfb single-producer/multi-consumer stress test
 *
 * Copyright (C) 2026 Markus Klotzbuecher <mk@mkio.de>
 *
 * SPDX-License-Identifier: MPL-2.0
 *
 * The producer (main thread) writes frames carrying a sequence
 * number and a position-dependent fill pattern into an lfb shm
 * segment. Each reader thread holds its own read-only mapping
 * (lfb_shm_open) and verifies that it receives every frame exactly
 * once, in order and uncorrupted.
 *
 * Writing proceeds in rounds of at most depth - LFB_CRUSH(depth)
 * frames; the producer waits for all readers to catch up before
 * starting the next round, so in this mode zero loss is provable
 * and any overrun is a test failure.
 *
 * In overrun mode (-O) the producer instead free-runs while a
 * deliberately slow reader gets lapped: here overruns *must* occur
 * and be reported, delivered frames must still be uncorrupted and
 * strictly ordered, and the reader must resync to the live stream.
 *
 * NOTE: uses the fixed shm segment "test-lfb" (leftovers from
 * aborted runs are cleaned up on start); don't run multiple
 * instances concurrently.
 */

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "lfb.h"
#include "lfb_shm.h"

#define SHM_NAME	"test-lfb"
#define MIN_FRAME_SIZE	16
#define STALL_TIMEOUT_S	10

/* how often and how long the overrun mode reader sleeps */
#define OVR_SLEEP_EVERY	200
#define OVR_SLEEP_US	10000

static long num_readers = 4;
static long num_msgs;		/* per round, 0 = auto */
static long num_rounds = 50;
static long depth = 10000;
static long frame_size = 64;
static int overrun_mode;
static int verbose;

static atomic_int readers_ready;
static atomic_int readers_done;
static atomic_int producer_done;

struct reader {
	pthread_t tid;
	int id;
	atomic_ulong consumed;
	unsigned long overruns;
	uint64_t last_seq;
};

static double tnow(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void mk_frame(uint8_t *f, uint64_t seq)
{
	memcpy(f, &seq, sizeof(seq));

	for (long i = sizeof(seq); i < frame_size; i++)
		f[i] = (uint8_t)('A' + (seq + (uint64_t)i) % 26);
}

/* verify integrity and (if expect >= 0) the exact sequence number */
static uint64_t check_frame(int rid, const uint8_t *f, int64_t expect)
{
	uint64_t seq;

	memcpy(&seq, f, sizeof(seq));

	if (expect >= 0 && seq != (uint64_t)expect) {
		fprintf(stderr,
			"reader %d: seq mismatch: got %"PRIu64", expected %"PRId64"\n",
			rid, seq, expect);
		exit(EXIT_FAILURE);
	}

	for (long i = sizeof(seq); i < frame_size; i++) {
		if (f[i] != (uint8_t)('A' + (seq + (uint64_t)i) % 26)) {
			fprintf(stderr,
				"reader %d: corrupt frame seq %"PRIu64" at byte %ld\n",
				rid, seq, i);
			exit(EXIT_FAILURE);
		}
	}

	return seq;
}

/* open the segment, retrying while the producer creates it */
static void reader_open(struct reader *r, lfb_shm_t *s)
{
	int ret;

	for (int i = 0; ; i++) {
		ret = lfb_shm_open(s, SHM_NAME);

		if (ret == 0)
			return;

		if (i > 5000) {
			fprintf(stderr, "reader %d: lfb_shm_open: %s\n",
				r->id, strerror(-ret));
			exit(EXIT_FAILURE);
		}

		usleep(1000);
	}
}

static void *reader_run(void *arg)
{
	struct reader *r = (struct reader *)arg;
	lfb_shm_t s;
	lfb_rd_t rd;
	uint64_t total = (uint64_t)num_rounds * (uint64_t)num_msgs;
	uint64_t expect = 0;
	uint8_t *buf = malloc(frame_size);
	int ret;

	if (buf == NULL)
		exit(EXIT_FAILURE);

	reader_open(r, &s);
	lfb_seek(lfb_shm_lfb(&s), &rd, LFB_OLDEST);
	atomic_fetch_add(&readers_ready, 1);

	while (expect < total) {
		ret = lfb_read(&rd, buf);

		if (ret == 0) {
			usleep(100);
			continue;
		}

		if (ret == -EPIPE) {
			fprintf(stderr,
				"reader %d: unexpected overrun at seq %"PRIu64"\n",
				r->id, expect);
			exit(EXIT_FAILURE);
		}

		check_frame(r->id, buf, (int64_t)expect);
		expect++;
		atomic_store(&r->consumed, expect);
	}

	if (rd.overruns != 0) {
		fprintf(stderr, "reader %d: %lu unexpected overruns\n",
			r->id, rd.overruns);
		exit(EXIT_FAILURE);
	}

	lfb_shm_close(&s);
	free(buf);
	atomic_fetch_add(&readers_done, 1);
	return NULL;
}

static void *reader_run_ovr(void *arg)
{
	struct reader *r = (struct reader *)arg;
	lfb_shm_t s;
	lfb_rd_t rd;
	uint64_t total = (uint64_t)num_rounds * (uint64_t)depth;
	uint64_t nread = 0, seq;
	int64_t last = -1;
	uint8_t *buf = malloc(frame_size);
	int ret;

	if (buf == NULL)
		exit(EXIT_FAILURE);

	reader_open(r, &s);
	lfb_seek(lfb_shm_lfb(&s), &rd, LFB_OLDEST);
	atomic_fetch_add(&readers_ready, 1);

	for (;;) {
		ret = lfb_read(&rd, buf);

		if (ret == 1) {
			if (lfb_lag(&rd) > (lfb_word_t)depth) {
				fprintf(stderr, "reader %d: lag > depth\n",
					r->id);
				exit(EXIT_FAILURE);
			}

			seq = check_frame(r->id, buf, -1);

			if ((int64_t)seq <= last) {
				fprintf(stderr,
					"reader %d: non-monotonic seq %"PRIu64" after %"PRId64"\n",
					r->id, seq, last);
				exit(EXIT_FAILURE);
			}

			last = (int64_t)seq;
			nread++;
			atomic_store(&r->consumed, nread);

			/* fall behind on purpose to force overruns */
			if (nread % OVR_SLEEP_EVERY == 0)
				usleep(OVR_SLEEP_US);

			if (seq == total - 1)
				break;
		} else if (ret == -EPIPE) {
			continue;	/* rd.overruns counts these */
		} else {
			if (atomic_load(&producer_done) && lfb_lag(&rd) == 0)
				break;
			usleep(100);
		}
	}

	r->overruns = rd.overruns;
	r->last_seq = (uint64_t)last;
	lfb_shm_close(&s);
	free(buf);
	atomic_fetch_add(&readers_done, 1);
	return NULL;
}

/* wait until all readers consumed at least target frames */
static void wait_consumed(struct reader *readers, uint64_t target)
{
	uint64_t min, lastmin = 0;
	double tlast = tnow();

	for (;;) {
		min = UINT64_MAX;

		for (long i = 0; i < num_readers; i++) {
			uint64_t c = atomic_load(&readers[i].consumed);

			min = (c < min) ? c : min;
		}

		if (min >= target)
			return;

		if (min != lastmin) {
			lastmin = min;
			tlast = tnow();
		} else if (tnow() - tlast > STALL_TIMEOUT_S) {
			fprintf(stderr,
				"stall: consumed %"PRIu64"/%"PRIu64" for %ds\n",
				min, target, STALL_TIMEOUT_S);
			exit(EXIT_FAILURE);
		}

		usleep(200);
	}
}

static void usage(const char *prog)
{
	printf("usage: %s [OPTIONS]\n"
	       "lfb single-producer/multi-consumer stress test\n"
	       "  -r NUM   number of reader threads (default: 4)\n"
	       "  -o NUM   messages per round (default: depth - crush zone)\n"
	       "  -n NUM   number of rounds (default: 50)\n"
	       "  -d NUM   ring depth in frames (default: 10000)\n"
	       "  -f NUM   frame size in bytes (>= %d, default: 64)\n"
	       "  -O       overrun mode: slow reader, overruns must occur\n"
	       "  -v       verbose\n"
	       "  -h       show this help\n",
	       prog, MIN_FRAME_SIZE);
}

int main(int argc, char **argv)
{
	int opt, ret;
	long max_msgs;
	lfb_shm_t prod;
	lfb_t *b;
	struct reader *readers;
	uint8_t *buf;
	uint64_t seq = 0, total;
	double t0, t1, dt;

	while ((opt = getopt(argc, argv, "r:o:n:d:f:Ovh")) != -1) {
		switch (opt) {
		case 'r': num_readers = atol(optarg); break;
		case 'o': num_msgs = atol(optarg); break;
		case 'n': num_rounds = atol(optarg); break;
		case 'd': depth = atol(optarg); break;
		case 'f': frame_size = atol(optarg); break;
		case 'O': overrun_mode = 1; break;
		case 'v': verbose = 1; break;
		case 'h': usage(argv[0]); return EXIT_SUCCESS;
		default: usage(argv[0]); return EXIT_FAILURE;
		}
	}

	if (num_readers < 1 || num_rounds < 1 || depth < 2 ||
	    (uint64_t)depth > (uint64_t)LFB_MAX_DEPTH ||
	    frame_size < MIN_FRAME_SIZE) {
		fprintf(stderr, "invalid parameters\n");
		return EXIT_FAILURE;
	}

	if (overrun_mode)
		num_readers = 1;

	max_msgs = depth - (long)LFB_CRUSH((lfb_word_t)depth);

	if (num_msgs == 0)
		num_msgs = max_msgs;

	if (num_msgs < 1 || num_msgs > max_msgs) {
		fprintf(stderr,
			"msgs per round (%ld) exceeds max round size %ld\n"
			"(a round must fit into the ring for zero loss to be verifiable;\n"
			"scale total volume via -n instead)\n",
			num_msgs, max_msgs);
		return EXIT_FAILURE;
	}

	total = overrun_mode ? (uint64_t)num_rounds * (uint64_t)depth :
		(uint64_t)num_rounds * (uint64_t)num_msgs;

	if (verbose)
		printf("readers: %ld, rounds: %ld, msgs/round: %ld, "
		       "depth: %ld, frame size: %ld, total: %"PRIu64"\n",
		       num_readers, num_rounds,
		       overrun_mode ? depth : num_msgs,
		       depth, frame_size, total);

	ret = lfb_shm_create(&prod, SHM_NAME,
			     (uint32_t)frame_size, (lfb_word_t)depth, 0);

	if (ret != 0) {
		fprintf(stderr, "lfb_shm_create: %s\n", strerror(-ret));
		return EXIT_FAILURE;
	}

	b = lfb_shm_lfb(&prod);
	buf = malloc(frame_size);
	readers = calloc(num_readers, sizeof(*readers));

	if (buf == NULL || readers == NULL)
		return EXIT_FAILURE;

	for (long i = 0; i < num_readers; i++) {
		readers[i].id = (int)i;
		ret = pthread_create(&readers[i].tid, NULL,
				     overrun_mode ? reader_run_ovr : reader_run,
				     &readers[i]);
		if (ret != 0) {
			fprintf(stderr, "pthread_create: %s\n", strerror(ret));
			return EXIT_FAILURE;
		}
	}

	t0 = tnow();

	while (atomic_load(&readers_ready) < num_readers) {
		if (tnow() - t0 > STALL_TIMEOUT_S) {
			fprintf(stderr, "readers failed to become ready\n");
			return EXIT_FAILURE;
		}
		usleep(100);
	}

	t0 = tnow();

	if (!overrun_mode) {
		for (long r = 0; r < num_rounds; r++) {
			for (long i = 0; i < num_msgs; i++) {
				mk_frame(buf, seq);
				lfb_write(b, buf);
				seq++;
			}

			wait_consumed(readers, seq);

			if (verbose)
				printf("round %ld/%ld done\n",
				       r + 1, num_rounds);
		}
	} else {
		for (seq = 0; seq < total; seq++) {
			mk_frame(buf, seq);
			lfb_write(b, buf);
		}
		atomic_store(&producer_done, 1);

		/* crude watchdog while the slow reader drains */
		double tlast = tnow();
		uint64_t lastc = 0;

		while (atomic_load(&readers_done) < num_readers) {
			uint64_t c = atomic_load(&readers[0].consumed);

			if (c != lastc) {
				lastc = c;
				tlast = tnow();
			} else if (tnow() - tlast > STALL_TIMEOUT_S) {
				fprintf(stderr, "stall: overrun reader stuck "
					"at %"PRIu64" frames\n", c);
				return EXIT_FAILURE;
			}
			usleep(1000);
		}
	}

	for (long i = 0; i < num_readers; i++)
		pthread_join(readers[i].tid, NULL);

	t1 = tnow();
	dt = t1 - t0;

	if (overrun_mode) {
		if (readers[0].overruns < 1) {
			fprintf(stderr,
				"expected overruns, but none occurred\n");
			return EXIT_FAILURE;
		}

		if (readers[0].last_seq != total - 1) {
			fprintf(stderr,
				"reader did not reach the final frame "
				"(%"PRIu64"/%"PRIu64")\n",
				readers[0].last_seq, total - 1);
			return EXIT_FAILURE;
		}

		printf("test_lfb: OK (overrun mode), elapsed: %.3f s, "
		       "%"PRIu64" written, %lu read intact, %lu overruns\n",
		       dt, total, atomic_load(&readers[0].consumed),
		       readers[0].overruns);
	} else {
		printf("test_lfb: OK, elapsed: %.3f s, %.0f msgs/s written, "
		       "%.0f msgs/s verified\n",
		       dt, (double)total / dt,
		       (double)total * (double)num_readers / dt);
	}

	free(buf);
	free(readers);
	lfb_shm_destroy(&prod);
	return EXIT_SUCCESS;
}
