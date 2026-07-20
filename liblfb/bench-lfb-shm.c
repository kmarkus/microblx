/*
 * bench-lfb-shm.c - multi-process shm latency/throughput benchmark
 *
 * Copyright (C) 2026 Markus Klotzbuecher <mk@mkio.de>
 *
 * SPDX-License-Identifier: MPL-2.0
 *
 * One producer process creates an lfb shm segment and forks N
 * reader processes, each of which opens its own read-only mapping
 * (lfb_shm_open) and drains the stream. Unlike test-lfb, which runs
 * the readers as threads of the producer, the consumers here are
 * genuinely separate address spaces, so what is measured includes
 * the real cross-process costs (separate page tables, TLB, IPI-free
 * but cache-coherent traffic between cores).
 *
 * Each frame carries a CLOCK_MONOTONIC timestamp taken immediately
 * before lfb_write; a reader's end-to-end latency is the difference
 * to the timestamp taken immediately after lfb_read returned the
 * frame. CLOCK_MONOTONIC is system-wide, so this is valid across
 * processes.
 *
 * The producer free-runs by default (-R 0). Note what that means
 * for the latency columns: a broadcast ring never blocks its
 * writer, so if the producer outruns the readers the ring stays
 * permanently backlogged and the reported latency is dominated by
 * queueing delay -- it then describes the reader's drain rate, not
 * delivery cost. Use the default to measure peak throughput and
 * overrun behaviour; pass -R below saturation to measure latency.
 *
 * Readers busy-poll by default (-y polls with a sleep instead,
 * which trades latency for CPU). Frames are not payload-verified
 * here; correctness is test-lfb's job.
 *
 * NOTE: uses the fixed shm segment "bench-lfb"; don't run multiple
 * instances concurrently.
 */

#define _GNU_SOURCE

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "lfb.h"
#include "lfb_shm.h"

#define SHM_NAME	"bench-lfb"
#define MIN_FRAME_SIZE	16	/* seq + tstamp */
#define STALL_TIMEOUT_S	10
#define READY_TIMEOUT_S	10

/* below this the pacer spins instead of sleeping */
#define PACE_SPIN_NS	60000

static long num_readers = 2;
static long num_msgs = 1000000;
static long depth = 8192;
static long frame_size = 64;
static long warmup = 10000;
static long rate;		/* msgs/s, 0 = free-run */
static long max_samples = 4000000;
static int pin_base = -1;	/* -1 = no pinning */
static int sleep_poll;
static int no_tstamp;
static int use_rslot;
static int verbose;

/* per-reader results, in shared anonymous memory */
struct rstat {
	uint64_t received;	/* frames delivered by lfb_read */
	uint64_t lost;		/* frames missed, from seq gaps */
	uint64_t overruns;	/* -EPIPE events */
	uint64_t samples;	/* latencies recorded (post-warmup) */
	uint64_t min, p50, p90, p99, p999, max;
	double mean;
	double elapsed;		/* first to last recorded frame */
	int stalled;
};

struct shared {
	atomic_int readers_ready;
	atomic_int producer_done;
	struct rstat rstat[];
};

static struct shared *sh;

static uint64_t now_ns(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/*
 * Collect one cpu per physical core into @cpus.
 *
 * Consecutive cpu ids are usually SMT siblings (cpu2 and cpu3 share
 * a core), so pinning naively to base + i puts the producer and the
 * first reader on the same physical core, where they compete for
 * one core's execution resources and the reader loses badly. Keep a
 * cpu only if it is the first entry of its own sibling list.
 *
 * @return number of cores found, 0 if the topology is unreadable
 */
static int core_list(int *cpus, int max)
{
	int n = 0;

	for (int cpu = 0; cpu < CPU_SETSIZE && n < max; cpu++) {
		char path[128], buf[128];
		FILE *f;
		int first;

		snprintf(path, sizeof(path),
			 "/sys/devices/system/cpu/cpu%d/topology/thread_siblings_list",
			 cpu);

		f = fopen(path, "r");

		if (f == NULL)
			continue;

		if (fgets(buf, sizeof(buf), f) != NULL &&
		    sscanf(buf, "%d", &first) == 1 && first == cpu)
			cpus[n++] = cpu;

		fclose(f);
	}

	return n;
}

/* pin to the @slot'th physical core at or after pin_base */
static void pin_to_slot(int slot)
{
	static int cpus[CPU_SETSIZE];
	static int ncores = -1;
	cpu_set_t set;
	int cpu;

	if (pin_base < 0)
		return;

	if (ncores < 0)
		ncores = core_list(cpus, CPU_SETSIZE);

	if (ncores == 0) {
		fprintf(stderr, "warning: cpu topology unreadable, "
			"not pinning\n");
		return;
	}

	cpu = cpus[(pin_base + slot) % ncores];

	CPU_ZERO(&set);
	CPU_SET(cpu, &set);

	if (sched_setaffinity(0, sizeof(set), &set) != 0)
		fprintf(stderr, "warning: pinning to cpu %d failed: %s\n",
			cpu, strerror(errno));
	else if (verbose)
		fprintf(stderr, "slot %d -> cpu %d\n", slot, cpu);
}

/* sleep until @deadline, spinning over the last PACE_SPIN_NS */
static void pace_until(uint64_t deadline)
{
	uint64_t now = now_ns();

	if (now >= deadline)
		return;

	if (deadline - now > PACE_SPIN_NS) {
		uint64_t wake = deadline - PACE_SPIN_NS;
		struct timespec ts = {
			.tv_sec = (time_t)(wake / 1000000000ull),
			.tv_nsec = (long)(wake % 1000000000ull),
		};

		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
	}

	while (now_ns() < deadline)
		;
}

static int cmp_u64(const void *a, const void *b)
{
	uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;

	return (x > y) - (x < y);
}

/* @s must be sorted ascending, @n > 0 */
static uint64_t pctl(const uint64_t *s, uint64_t n, double p)
{
	uint64_t i = (uint64_t)(p * (double)(n - 1) + 0.5);

	return s[i];
}

/* open the segment, retrying while the producer creates it */
static void reader_open(int id, lfb_shm_t *s)
{
	int ret;

	for (int i = 0; ; i++) {
		ret = lfb_shm_open(s, SHM_NAME);

		if (ret == 0)
			return;

		if (i > 5000) {
			fprintf(stderr, "reader %d: lfb_shm_open: %s\n",
				id, strerror(-ret));
			_exit(EXIT_FAILURE);
		}

		usleep(1000);
	}
}

/* runs in the forked child; never returns */
static void reader_run(int id, lfb_shm_t *inherited)
{
	struct rstat *st = &sh->rstat[id];
	lfb_shm_t s;
	lfb_rd_t rd;
	uint8_t *buf;
	uint64_t *lat;
	uint64_t nrecv = 0, nlat = 0, lost = 0;
	uint64_t first_ns = 0, last_ns = 0;
	uint64_t last_progress;
	int64_t expect = -1;
	int done = 0;

	/*
	 * drop the producer's read-write mapping inherited across the
	 * fork: this process is a consumer and must reach the segment
	 * the way any unrelated process would
	 */
	lfb_shm_close(inherited);

	pin_to_slot(1 + id);

	buf = malloc((size_t)frame_size);
	lat = malloc((size_t)max_samples * sizeof(*lat));

	if (buf == NULL || lat == NULL) {
		fprintf(stderr, "reader %d: out of memory\n", id);
		_exit(EXIT_FAILURE);
	}

	reader_open(id, &s);
	lfb_seek(lfb_shm_lfb(&s), &rd, LFB_NEWEST);

	atomic_fetch_add(&sh->readers_ready, 1);
	last_progress = now_ns();

	for (;;) {
		uint64_t t;
		uint64_t seq = 0, tstamp = 0;
		int ret;

		if (use_rslot) {
			const void *f;

			ret = lfb_get_rslot(&rd, &f);

			if (ret == 1) {
				/*
				 * provisional until the commit below
				 * confirms the producer did not overwrite
				 * the slot while we read it: take only the
				 * 16 header bytes we need, never the whole
				 * frame -- that is the point of the borrow
				 */
				memcpy(&seq, f, sizeof(seq));
				memcpy(&tstamp, (const uint8_t *)f +
				       sizeof(seq), sizeof(tstamp));

				ret = lfb_check_rslot(&rd);
			}
		} else {
			ret = lfb_read(&rd, buf);

			if (ret == 1) {
				memcpy(&seq, buf, sizeof(seq));
				memcpy(&tstamp, buf + sizeof(seq),
				       sizeof(tstamp));
			}
		}

		if (ret == -EPIPE)
			continue;	/* counted via rd.overruns */

		if (ret == 0) {
			if (done)
				break;

			/*
			 * load the flag *after* a read came up empty, then
			 * make one more pass: lfb_read's acquire load of w
			 * then happens after the producer's last write
			 */
			done = atomic_load(&sh->producer_done);

			if (done)
				continue;

			if (now_ns() - last_progress >
			    (uint64_t)STALL_TIMEOUT_S * 1000000000ull) {
				st->stalled = 1;
				break;
			}

			if (sleep_poll)
				usleep(50);

			continue;
		}

		nrecv++;
		memcpy(&seq, buf, sizeof(seq));

		if (expect >= 0 && (int64_t)seq > expect)
			lost += seq - (uint64_t)expect;

		expect = (int64_t)seq + 1;

		/*
		 * The clock_gettime below is per frame and does not
		 * shrink with frame_size, so at small frames it is a
		 * significant share of the reader's per-frame cost --
		 * i.e. the benchmark then partly measures itself. -T
		 * drops it to get an unperturbed drain rate.
		 */
		if (no_tstamp) {
			if ((nrecv & 1023) == 0)
				last_progress = now_ns();

			continue;
		}

		t = now_ns();
		last_progress = t;

		if (nrecv > (uint64_t)warmup && nlat < (uint64_t)max_samples) {
			lat[nlat++] = t > tstamp ? t - tstamp : 0;

			if (nlat == 1)
				first_ns = t;

			last_ns = t;
		}
	}

	st->received = nrecv;
	st->lost = lost;
	st->overruns = rd.overruns;
	st->samples = nlat;

	if (nlat > 0) {
		double sum = 0;

		for (uint64_t i = 0; i < nlat; i++)
			sum += (double)lat[i];

		st->mean = sum / (double)nlat;
		st->elapsed = (double)(last_ns - first_ns) / 1e9;

		qsort(lat, nlat, sizeof(*lat), cmp_u64);

		st->min = lat[0];
		st->p50 = pctl(lat, nlat, 0.50);
		st->p90 = pctl(lat, nlat, 0.90);
		st->p99 = pctl(lat, nlat, 0.99);
		st->p999 = pctl(lat, nlat, 0.999);
		st->max = lat[nlat - 1];
	}

	lfb_shm_close(&s);
	free(buf);
	free(lat);
	_exit(EXIT_SUCCESS);
}

static void usage(const char *argv0)
{
	printf("usage: %s [options]\n"
	       "  -r N   reader processes (default %ld)\n"
	       "  -n N   frames to send (default %ld)\n"
	       "  -d N   ring depth in frames (default %ld)\n"
	       "  -f N   frame size in bytes (default %ld, min %d)\n"
	       "  -R N   target rate in msgs/s (default %ld, 0 = free-run)\n"
	       "  -w N   warmup frames excluded from stats (default %ld)\n"
	       "  -s N   max latency samples per reader (default %ld)\n"
	       "  -p N   pin producer to physical core N, readers to N+1..\n"
	       "         (distinct cores, SMT siblings skipped; default off)\n"
	       "  -y     sleep-poll in readers instead of busy-polling\n"
	       "  -T     no per-frame timestamping (throughput only, no latency)\n"
	       "  -P     zero-copy readers (lfb_get_rslot) instead of lfb_read\n"
	       "  -v     verbose\n"
	       "  -h     this help\n",
	       argv0, num_readers, num_msgs, depth, frame_size,
	       MIN_FRAME_SIZE, rate, warmup, max_samples);
}

int main(int argc, char **argv)
{
	lfb_shm_t prod;
	lfb_t *b;
	pid_t *pids;
	uint8_t *buf;
	size_t shsz;
	uint64_t t0, t1, period = 0;
	double dt;
	int opt, ret, rc = EXIT_SUCCESS;

	while ((opt = getopt(argc, argv, "r:n:d:f:R:w:s:p:yTPvh")) != -1) {
		switch (opt) {
		case 'r': num_readers = atol(optarg); break;
		case 'n': num_msgs = atol(optarg); break;
		case 'd': depth = atol(optarg); break;
		case 'f': frame_size = atol(optarg); break;
		case 'R': rate = atol(optarg); break;
		case 'w': warmup = atol(optarg); break;
		case 's': max_samples = atol(optarg); break;
		case 'p': pin_base = atoi(optarg); break;
		case 'y': sleep_poll = 1; break;
		case 'T': no_tstamp = 1; break;
		case 'P': use_rslot = 1; break;
		case 'v': verbose = 1; break;
		case 'h': usage(argv[0]); return EXIT_SUCCESS;
		default: usage(argv[0]); return EXIT_FAILURE;
		}
	}

	if (num_readers < 1 || num_msgs < 1 || depth < 2 ||
	    (uint64_t)depth > (uint64_t)LFB_MAX_DEPTH ||
	    frame_size < MIN_FRAME_SIZE || warmup < 0 ||
	    max_samples < 1 || rate < 0) {
		fprintf(stderr, "invalid parameters\n");
		return EXIT_FAILURE;
	}

	if (warmup >= num_msgs) {
		fprintf(stderr, "warmup (%ld) must be below -n (%ld)\n",
			warmup, num_msgs);
		return EXIT_FAILURE;
	}

	if (num_msgs - warmup < max_samples)
		max_samples = num_msgs - warmup;

	if (rate > 0)
		period = 1000000000ull / (uint64_t)rate;

	shsz = sizeof(*sh) + (size_t)num_readers * sizeof(struct rstat);
	sh = mmap(NULL, shsz, PROT_READ | PROT_WRITE,
		  MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	if (sh == MAP_FAILED) {
		fprintf(stderr, "mmap: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	memset(sh, 0, shsz);

	ret = lfb_shm_create(&prod, SHM_NAME,
			     (uint32_t)frame_size, (lfb_word_t)depth, 0);

	if (ret != 0) {
		fprintf(stderr, "lfb_shm_create: %s\n", strerror(-ret));
		return EXIT_FAILURE;
	}

	b = lfb_shm_lfb(&prod);
	buf = calloc(1, (size_t)frame_size);
	pids = calloc((size_t)num_readers, sizeof(*pids));

	if (buf == NULL || pids == NULL) {
		fprintf(stderr, "out of memory\n");
		lfb_shm_destroy(&prod);
		return EXIT_FAILURE;
	}

	if (verbose)
		printf("readers: %ld, frames: %ld, depth: %ld, "
		       "frame size: %ld, rate: %ld%s\n",
		       num_readers, num_msgs, depth, frame_size, rate,
		       rate ? "/s" : " (free-run)");

	for (long i = 0; i < num_readers; i++) {
		pids[i] = fork();

		if (pids[i] < 0) {
			fprintf(stderr, "fork: %s\n", strerror(errno));
			lfb_shm_destroy(&prod);
			return EXIT_FAILURE;
		}

		if (pids[i] == 0)
			reader_run((int)i, &prod);	/* no return */
	}

	pin_to_slot(0);

	t0 = now_ns();

	while (atomic_load(&sh->readers_ready) < num_readers) {
		if (now_ns() - t0 >
		    (uint64_t)READY_TIMEOUT_S * 1000000000ull) {
			fprintf(stderr, "readers failed to become ready\n");
			lfb_shm_destroy(&prod);
			return EXIT_FAILURE;
		}

		usleep(100);
	}

	/*
	 * readers start at LFB_NEWEST, so nothing written before this
	 * point is delivered; the ready barrier above is what makes
	 * the first frames measurable rather than dropped
	 */
	t0 = now_ns();

	for (long i = 0; i < num_msgs; i++) {
		uint64_t seq = (uint64_t)i, ts;

		if (period)
			pace_until(t0 + (uint64_t)i * period);

		ts = now_ns();
		memcpy(buf, &seq, sizeof(seq));
		memcpy(buf + sizeof(seq), &ts, sizeof(ts));
		lfb_write(b, buf);
	}

	t1 = now_ns();
	atomic_store(&sh->producer_done, 1);

	for (long i = 0; i < num_readers; i++) {
		int status;

		if (waitpid(pids[i], &status, 0) < 0) {
			fprintf(stderr, "waitpid: %s\n", strerror(errno));
			rc = EXIT_FAILURE;
			continue;
		}

		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
			fprintf(stderr, "reader %ld failed (status %d)\n",
				i, status);
			rc = EXIT_FAILURE;
		}
	}

	dt = (double)(t1 - t0) / 1e9;

	printf("bench-lfb-shm: %ld readers, %ld frames of %ld B, depth %ld, ",
	       num_readers, num_msgs, frame_size, depth);

	if (rate)
		printf("paced at %ld msgs/s\n", rate);
	else
		printf("free-run\n");

	printf("producer: %.3f s, %.0f msgs/s, %.1f MiB/s\n",
	       dt, (double)num_msgs / dt,
	       (double)num_msgs * (double)frame_size / dt / (1024 * 1024));

	printf("\n%-4s %12s %10s %8s %9s %9s %9s %9s %9s\n",
	       "rdr", "received", "lost", "ovr",
	       "min", "p50", "p90", "p99", "max");

	for (long i = 0; i < num_readers; i++) {
		struct rstat *st = &sh->rstat[i];

		printf("%-4ld %12"PRIu64" %10"PRIu64" %8"PRIu64
		       " %9"PRIu64" %9"PRIu64" %9"PRIu64" %9"PRIu64
		       " %9"PRIu64"%s\n",
		       i, st->received, st->lost, st->overruns,
		       st->min, st->p50, st->p90, st->p99, st->max,
		       st->stalled ? "  STALLED" : "");
	}

	printf("\nlatencies in ns, sampled after %ld warmup frames "
	       "(at most %ld samples/reader)\n", warmup, max_samples);

	if (verbose) {
		for (long i = 0; i < num_readers; i++)
			printf("reader %ld: %"PRIu64" samples, mean %.0f ns, "
			       "p99.9 %"PRIu64" ns, drain %.0f msgs/s\n",
			       i, sh->rstat[i].samples,
			       sh->rstat[i].mean, sh->rstat[i].p999,
			       sh->rstat[i].elapsed > 0 ?
			       (double)sh->rstat[i].samples /
			       sh->rstat[i].elapsed : 0);
	}

	free(buf);
	free(pids);
	lfb_shm_destroy(&prod);
	munmap(sh, shsz);
	return rc;
}
