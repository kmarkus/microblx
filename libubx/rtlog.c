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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
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
	msg.src[UBX_BLOCK_NAME_MAXLEN] = '\0';

	va_start(args, fmt);
	vsnprintf(msg.msg, sizeof(msg.msg), fmt, args);
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
	int shm_fd;
	uint32_t shm_size;
	uint32_t frame_size;

	log_buf_t *buf_ptr;	/* ptr to the shm region */
};

struct log_shm_inf inf;

/**
 * log_inc_woff - advance and publish the write offset
 *
 * Must be called with wlock held (there may be concurrent writers).
 * The release store pairs with the readers' acquire loads: a reader
 * observing the new offset is guaranteed to see the frame written
 * before it.
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
	next.wrap_off = atomic_load_explicit(&inf.buf_ptr->w,
					     memory_order_relaxed);
	next.off += inc;

	if (next.off > inf.shm_size - inf.frame_size - sizeof(log_buf_t)) {
		next.off = 0;
		next.wrap++;
	}

	atomic_store_explicit(&inf.buf_ptr->w, next.wrap_off,
			      memory_order_release);
}

static void ubx_log_shm(const struct ubx_node *nd, const struct ubx_log_msg *msg)
{
	int ret;
	log_wrap_off_t w;
	struct ubx_log_msg *frame;
	(void)(nd);

	ret = pthread_mutex_lock(&inf.buf_ptr->wlock);

	if (ret == EOWNERDEAD) {
		/*
		 * the previous owner died holding the lock. The header
		 * is still consistent, since w is only advanced after
		 * the frame is complete; at worst a partial frame at
		 * w.off gets overwritten now.
		 */
		pthread_mutex_consistent(&inf.buf_ptr->wlock);
	} else if (ret != 0) {
		return;	/* ENOTRECOVERABLE: drop the message */
	}

	w.wrap_off = atomic_load_explicit(&inf.buf_ptr->w,
					  memory_order_relaxed);
	frame = (struct ubx_log_msg *)&inf.buf_ptr->data[w.off];
	memcpy(frame, msg, sizeof(struct ubx_log_msg));

	log_inc_woff(inf.frame_size);
	pthread_mutex_unlock(&inf.buf_ptr->wlock);
}

int ubx_log_init(struct ubx_node *nd)
{
	int ret = -1;
	int need_init = 1;
	struct stat sb;

	nd->log_data = NULL;

	inf.shm_size = sizeof(log_buf_t) + sizeof(struct ubx_log_msg) *
		       LOG_BUFFER_DEPTH;

	inf.frame_size = sizeof(struct ubx_log_msg);

	/* allocate shared mem. Try to create it first (O_EXCL), so
	 * that only the creator initializes the mutex and write
	 * offset. Re-initializing the lock of an existing segment
	 * would corrupt it if another process is logging. */
	inf.shm_fd = shm_open(LOG_SHM_FILENAME,
			      O_CREAT | O_EXCL | O_RDWR, 0640);

	if (inf.shm_fd == -1 && errno == EEXIST) {
		need_init = 0;
		inf.shm_fd = shm_open(LOG_SHM_FILENAME, O_RDWR, 0640);
	}

	if (inf.shm_fd == -1) {
		fprintf(stderr, "%s: shm_open failed: %m\n", __func__);
		goto out;
	}

	/* check if we need to adjust size, otherwise leave it */
	if (fstat(inf.shm_fd, &sb) != 0) {
		fprintf(stderr, "%s: fstat shm failed: %m\n", __func__);
		goto out_unlink;
	}

	if (sb.st_size != (off_t) inf.shm_size) {
		ret = ftruncate(inf.shm_fd, inf.shm_size);

		if (ret != 0) {
			fprintf(stderr, "%s: resizing shm failed: %m\n", __func__);
			goto out_unlink;
		}
		/* resized: header is in an unknown state */
		need_init = 1;
	}

	inf.buf_ptr = mmap(0, inf.shm_size,
			    PROT_READ | PROT_WRITE,
			    MAP_SHARED, inf.shm_fd, 0);

	if (inf.buf_ptr == MAP_FAILED) {
		ret = -1;
		fprintf(stderr, "%s: mmap shm failed: %m\n", __func__);
		goto out_unlink;
	}

	if (need_init) {
		pthread_mutexattr_t mattr;

		pthread_mutexattr_init(&mattr);
		pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
		pthread_mutexattr_setprotocol(&mattr, PTHREAD_PRIO_INHERIT);
		pthread_mutexattr_setrobust(&mattr, PTHREAD_MUTEX_ROBUST);

		ret = pthread_mutex_init(&inf.buf_ptr->wlock, &mattr);
		pthread_mutexattr_destroy(&mattr);

		if (ret != 0) {
			fprintf(stderr, "%s: mutex init failed: %s\n",
				__func__, strerror(ret));
			munmap(inf.buf_ptr, inf.shm_size);
			ret = -1;
			goto out_unlink;
		}

		atomic_store_explicit(&inf.buf_ptr->w, 0,
				      memory_order_relaxed);
	}

	nd->log = ubx_log_shm;

	ret = 0;
	goto out;

out_unlink:
	shm_unlink(LOG_SHM_FILENAME);
	close(inf.shm_fd);
out:
	return ret;
}

void ubx_log_cleanup(struct ubx_node *nd)
{
	/* we skip destroying the mutex and shm, since there may be
	 * other processes still using it */
	nd->log = NULL;
	munmap(inf.buf_ptr, inf.shm_size);
	close(inf.shm_fd);
}
#endif
