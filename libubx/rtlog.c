/*
 * microblx real-time logging support
 *
 * Copyright (C) 2019 Hamish Guthrie <hamish.guthrie@kistler.com>
 * Copyright (C) 2019-2020 Markus Klotzbuecher <mk@mkio.de>
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <stdarg.h>
#include <stdio.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <pthread.h>

#include <config.h>

#include "ubx.h"
#include "rtlog.h"

const char *loglevel_str[] = {
	"EMERG", "ALERT", "CRIT", "ERROR",
	"WARN", "NOTICE", "INFO", "DEBUG"
};

#undef CONFIG_SIMPLE_LOGGING
#define CONFIG_LOGGING_SHM

/* basic logging function */
void __ubx_log(const int level, const ubx_node_t *nd, const char *src, const char *fmt, ...)
{
	va_list args;
	struct ubx_log_msg msg;

	ubx_gettime(&msg.ts);
	msg.level = level;

	strncpy(msg.src, src, UBX_BLOCK_NAME_MAXLEN);

	va_start(args, fmt);
	vsnprintf(msg.msg, UBX_LOG_MSG_MAXLEN, fmt, args);
	va_end(args);

	if (!nd->log) {
		fprintf(stderr,
			"ERROR: rtlog: node->log is NULL (msg: %s: %s)\n",
			msg.src, msg.msg);
		return;
	}

	nd->log(nd, &msg);

	return;

}

#ifdef CONFIG_SIMPLE_LOGGING
static void ubx_log_simple(const struct ubx_node *nd, const struct ubx_log_msg *msg)
{
	FILE *stream;
	const char *level_str;

	stream = (msg->level <= UBX_LOGLEVEL_WARN) ? stderr : stdout;

	level_str = (msg->level > UBX_LOGLEVEL_DEBUG ||
		     msg->level < UBX_LOGLEVEL_EMERG) ?
		"INVALID" : loglevel_str[msg->level];

	fprintf(stream, "[%li.%06li] %s %s.%s: %s\n",
		msg->ts.sec, msg->ts.nsec / NSEC_PER_USEC,
		level_str, nd->name, msg->src, msg->msg);
}

int ubx_log_init(struct ubx_node *nd)
{
	nd->log = ubx_log_simple;
	nd->log_data = NULL;
	return 0;
}

void ubx_log_cleanup(struct ubx_node *nd)
{
	nd->log = NULL;
}
#endif

#ifdef CONFIG_LOGGING_SHM

#include "internal/rtlog_common.h"

struct log_shm_inf {
	pthread_spinlock_t loglock;
	int shm_fd;
	uint32_t shm_size;
	uint32_t frame_size;

	volatile log_buf_t *buf_ptr;	/* ptr to the shm region */
};

struct log_shm_inf inf;

/**
 * log_inc_woff - atomically inc the write offset
 *
 * This works, because
 *  - aligned memory accesses are guaranteed atomic on most architectures
 *  - we only have one writer
 *
 * This be portable, this should use gcc atomic ops.
 *
 * @param inc new write pointer offset (bytes)
 */
static void log_inc_woff(uint32_t inc)
{
	log_wrap_off_t next;

	/*
	 * read current wrap and write offset to preserve wrap
	 * counter
	 */
	next = inf.buf_ptr->w;
	next.off += inc;

	if (next.off > inf.shm_size - inf.frame_size - sizeof(log_buf_t)) {
		next.off = 0;
		next.wrap++;
	}

	/* atomic */
	inf.buf_ptr->w = next;
}

static void ubx_log_shm(const struct ubx_node *nd, const struct ubx_log_msg *msg)
{
	struct ubx_log_msg *frame;
	(void)(nd);

	pthread_spin_lock(&inf.loglock);
	frame = (struct ubx_log_msg *)&inf.buf_ptr->data[inf.buf_ptr->w.off];
	memcpy((void *)frame, (void *)msg, sizeof(struct ubx_log_msg));

	log_inc_woff(inf.frame_size);
	pthread_spin_unlock(&inf.loglock);
}

int ubx_log_init(struct ubx_node *nd)
{
	int ret = -1;

	pthread_spin_init(&inf.loglock, PTHREAD_PROCESS_SHARED);
	nd->log = ubx_log_shm;
	nd->log_data = NULL;

	inf.shm_size = sizeof(log_wrap_off_t) + sizeof(struct ubx_log_msg) *
		       LOG_BUFFER_DEPTH;
	inf.frame_size = sizeof(struct ubx_log_msg);

	/* allocate shared mem */

	inf.shm_fd = shm_open(LOG_SHM_FILENAME, O_CREAT | O_RDWR, 0640);

	if (inf.shm_fd == -1) {
		fprintf(stderr, "%s: shm_open failed: %m\n", __func__);
		goto out;
	}

	ret = ftruncate(inf.shm_fd, inf.shm_size);
	if (ret != 0) {
		fprintf(stderr, "%s: resizing shm failed: %m\n", __func__);
		goto out_close;
	}

	inf.buf_ptr = mmap(0, inf.shm_size,
			    PROT_READ | PROT_WRITE,
			    MAP_SHARED, inf.shm_fd, 0);

	if (inf.buf_ptr == MAP_FAILED) {
		ret = -1;
		fprintf(stderr, "%s: mmap shm failed: %m\n", __func__);
		goto out_unlink;
	}

	inf.frame_size = sizeof(struct ubx_log_msg);

	ret = 0;
	goto out;

out_unlink:
	shm_unlink(LOG_SHM_FILENAME);

out_close:
	close(inf.shm_fd);

out:
	return ret;
}

void ubx_log_cleanup(struct ubx_node *nd)
{
	pthread_spin_destroy(&inf.loglock);
	nd->log = NULL;

	/* clean up shm */
	munmap((void *) inf.buf_ptr, inf.shm_size);

	shm_unlink(LOG_SHM_FILENAME);

	close(inf.shm_fd);
}
#endif
