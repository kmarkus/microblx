/*
 * test-lfbm: lfbm multi-producer stress test
 *
 * Copyright (C) 2026 Markus Klotzbuecher <mk@mkio.de>
 *
 * SPDX-License-Identifier: MPL-2.0
 *
 * Forked writers hammer one ring in shared memory, forked readers
 * check that every delivered frame is intact and in per-writer order.
 * The ring is far too small to keep up, so overruns are expected and
 * this exercises overwriting, unlike tools/test-rtlog; use a small
 * depth to have readers race the writers of the same slot. Half-way
 * through a writer is SIGKILLed, possibly mid-write, which must not
 * affect the others. With -T all run as threads instead, for
 * ThreadSanitizer, and nobody gets killed.
 */

#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

/* a killed writer's slot, or one of a loaded box: don't wait long */
#define LFBM_STALL_NS 100000000ULL

#include "lfbm.h"

#define FILL_WORDS	12

struct frame {
	uint64_t writer;
	uint64_t seq;
	uint64_t fill[FILL_WORDS];
};

struct shared {
	_Atomic int stop;
	_Atomic unsigned long frames;
	_Atomic unsigned long skipped;
	_Atomic unsigned long overruns;
};

static struct shared *sh;
static lfb_t *b;
static int num_writers = 8, num_readers = 4, seconds = 2, depth = 1024;
static int threads;

static void writer(int id)
{
	struct frame f = { .writer = (uint64_t)id };

	while (!atomic_load_explicit(&sh->stop, memory_order_relaxed)) {
		f.seq++;

		for (int i = 0; i < FILL_WORDS; i++)
			f.fill[i] = f.seq * (uint64_t)(id + i + 1);

		if (lfbm_write(b, &f) != 0)
			f.seq--;
	}
}

static int verify(const struct frame *f, uint64_t *last)
{
	if (f->writer >= (uint64_t)num_writers) {
		fprintf(stderr, "bad writer id %lu\n", (unsigned long)f->writer);
		return -1;
	}

	for (int i = 0; i < FILL_WORDS; i++) {
		if (f->fill[i] != f->seq * (f->writer + (uint64_t)i + 1)) {
			fprintf(stderr, "torn frame: w%lu seq %lu word %d\n",
				(unsigned long)f->writer,
				(unsigned long)f->seq, i);
			return -1;
		}
	}

	if (f->seq <= last[f->writer]) {
		fprintf(stderr, "w%lu out of order: %lu after %lu\n",
			(unsigned long)f->writer, (unsigned long)f->seq,
			(unsigned long)last[f->writer]);
		return -1;
	}

	last[f->writer] = f->seq;
	return 0;
}

static void reader(void)
{
	uint64_t last[256] = { 0 };
	lfbm_rd_t rd;
	struct frame f;
	unsigned long cnt = 0;

	lfbm_rd_init(b, &rd, LFB_OLDEST);

	while (!atomic_load_explicit(&sh->stop, memory_order_relaxed)) {
		if (lfbm_read(&rd, &f) != 1)
			continue;

		if (verify(&f, last) != 0) {
			atomic_store(&sh->stop, 1);
			exit(1);
		}

		cnt++;
	}

	atomic_fetch_add(&sh->frames, cnt);
	atomic_fetch_add(&sh->skipped, rd.skipped);
	atomic_fetch_add(&sh->overruns, rd.overruns);
}

static void *thread_main(void *arg)
{
	int id = (int)(intptr_t)arg;

	if (id < num_writers)
		writer(id);
	else
		reader();

	return NULL;
}

static void run_threads(void)
{
	pthread_t t[256 * 2];
	int n = num_writers + num_readers;

	for (int i = 0; i < n; i++) {
		if (pthread_create(&t[i], NULL, thread_main,
				   (void *)(intptr_t)i) != 0) {
			perror("pthread_create");
			exit(1);
		}
	}

	sleep((unsigned int)seconds);
	atomic_store(&sh->stop, 1);

	for (int i = 0; i < n; i++)
		pthread_join(t[i], NULL);
}

static void usage(const char *prog)
{
	printf("usage: %s [-T] [-w writers] [-r readers] [-s seconds] [-d depth]\n",
	       prog);
}

int main(int argc, char **argv)
{
	size_t sz;
	pid_t pids[256 * 2];
	int opt, np = 0, status, ret = 0;
	void *mem;

	while ((opt = getopt(argc, argv, "Tw:r:s:d:h")) != -1) {
		switch (opt) {
		case 'T': threads = 1; break;
		case 'w': num_writers = atoi(optarg); break;
		case 'r': num_readers = atoi(optarg); break;
		case 's': seconds = atoi(optarg); break;
		case 'd': depth = atoi(optarg); break;
		default:
			usage(argv[0]);
			exit(opt == 'h' ? 0 : 1);
		}
	}

	sz = lfbm_memsz((uint32_t)sizeof(struct frame), (lfb_word_t)depth, 0);

	if (num_writers < 2 || num_writers > 256 ||
	    num_readers < 1 || num_readers > 256 || sz == 0) {
		usage(argv[0]);
		exit(1);
	}

	sh = mmap(NULL, sizeof(*sh), PROT_READ | PROT_WRITE,
		  MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	mem = mmap(NULL, sz, PROT_READ | PROT_WRITE,
		   MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	if (sh == MAP_FAILED || mem == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	/* fill with junk: lfbm_init must not depend on zeroed memory */
	memset(mem, 0xa5, sz);
	b = lfbm_init(mem, sz, (uint32_t)sizeof(struct frame),
		      (lfb_word_t)depth, 0);

	if (b == NULL) {
		fprintf(stderr, "lfbm_init failed\n");
		exit(1);
	}

	if (threads) {
		run_threads();
		goto out;
	}

	for (int i = 0; i < num_writers + num_readers; i++) {
		pid_t pid = fork();

		if (pid == 0) {
			if (i < num_writers)
				writer(i);
			else
				reader();
			_exit(0);
		}

		if (pid == -1) {
			perror("fork");
			ret = 1;
			break;
		}

		pids[np++] = pid;
	}

	if (ret == 0) {
		usleep((useconds_t)seconds * 500000);
		kill(pids[0], SIGKILL);
		usleep((useconds_t)seconds * 500000);
	}

	atomic_store(&sh->stop, 1);

	for (int i = 0; i < np; i++) {
		if (waitpid(pids[i], &status, 0) != pids[i]) {
			perror("waitpid");
			ret = 1;
			continue;
		}

		/* writer 0 was killed */
		if (i > 0 && (!WIFEXITED(status) || WEXITSTATUS(status) != 0))
			ret = 1;
	}

out:
	if (atomic_load(&sh->frames) == 0)
		ret = 1;

	printf("frames: %lu, overruns: %lu, skipped: %lu: %s\n",
	       atomic_load(&sh->frames), atomic_load(&sh->overruns),
	       atomic_load(&sh->skipped), ret ? "FAILED" : "OK");

	return ret;
}
