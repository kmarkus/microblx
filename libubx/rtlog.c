/*
 * microblx real-time logging support
 *
 * Copyright (C) 2019 Hamish Guthrie <hamish.guthrie@kistler.com>
 * Copyright (C) 2019-2020 Markus Klotzbuecher <mk@mkio.de>
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <stdarg.h>
#include <inttypes.h>
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
	struct ubx_timespec ts;

	ubx_gettime(&ts);
	msg.ts = (int64_t)ubx_ts_to_ns(&ts);
	msg.level = level;

	strncpy(msg.src, src, UBX_BLOCK_NAME_MAXLEN);
	msg.src[UBX_BLOCK_NAME_MAXLEN] = '\0';

	va_start(args, fmt);
	/* mark truncated messages with a trailing '~' */
	if (vsnprintf(msg.msg, sizeof(msg.msg), fmt, args) >= (int)sizeof(msg.msg))
		msg.msg[sizeof(msg.msg) - 2] = '~';
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

	fprintf(stream, "[%" PRId64 ".%06" PRId64 "] %s %s.%s: %s\n",
		msg->ts / NSEC_PER_SEC,
		(msg->ts % NSEC_PER_SEC) / NSEC_PER_USEC,
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
#include "lfbm_shm.h"

/*
 * A log slot (frame + lfbm sequence word) must stay a multiple of the
 * 64 byte cache line, so that a writer filling one slot never dirties
 * a line another writer or a reader is using for the neighbouring
 * slot. If this fires, adjust the UBX_LOG_MSG_MAXLEN CMake cache
 * variable (values 64*k - 85, e.g. 43, 107, 171) for this target -- it
 * cannot be asserted in ubx_types.h, which has to remain luajit-ffi
 * parsable.
 */
_Static_assert(LOG_SLOT_SZ % 64 == 0,
	       "log slot must be a multiple of the 64 byte cache line");

/* slot 0 starts on a cache line, given a page aligned mapping */
_Static_assert((LFB_HDR_SZ + sizeof(log_user_t)) % 64 == 0,
	       "log user area must align the slots to the cache line");

/* bound the retries when another party is mid-create (see log_join) */
#define LOG_JOIN_RETRIES	1000
#define LOG_JOIN_RETRY_US	1000

/* the segment is mapped once per process and shared by all nodes:
 * joined by the first ubx_log_init, closed by the last cleanup */
static lfb_shm_t log_shm;
static log_user_t *log_user;
static int log_nodes;
static pthread_mutex_t log_nodes_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * log_join - create or attach to the log segment
 *
 * lfbm_shm_join creates the segment if absent, or attaches read-write
 * to an existing one after checking it matches our geometry exactly.
 *
 * @return 1 if created (caller must publish), 0 if an existing segment
 *         was joined, negative errno on failure
 */
static int log_join(void)
{
	int ret, unlinked = 0;

	for (int i = 0; i < LOG_JOIN_RETRIES; i++) {
		ret = lfbm_shm_join(&log_shm, LOG_SHM_FILENAME,
				    sizeof(struct ubx_log_msg),
				    LOG_BUFFER_DEPTH, sizeof(log_user_t));

		if (ret >= 0)
			return ret;

		if (ret == -EAGAIN) {
			/* somebody is mid-create: give them a moment */
			usleep(LOG_JOIN_RETRY_US);
			continue;
		}

		/*
		 * a segment of different geometry is in the way, e.g.
		 * left by a node built against another log message
		 * layout. Discard it once: consumers keep their
		 * mapping until they notice it went stale.
		 */
		if (ret == -EPROTO && !unlinked) {
			shm_unlink(LOG_SHM_FILENAME);
			unlinked = 1;
			continue;
		}

		return ret;
	}

	return -ETIMEDOUT;
}

static void ubx_log_shm(const struct ubx_node *nd, const struct ubx_log_msg *msg)
{
	(void)(nd);

	/* too many writers stalled at once */
	if (lfbm_write(lfb_shm_lfb(&log_shm), msg) != 0)
		atomic_fetch_add_explicit(&log_user->dropped, 1,
					  memory_order_relaxed);
}

static int log_open(void)
{
	int ret;

	ret = log_join();

	if (ret < 0) {
		fprintf(stderr, "%s: joining %s failed: %s\n",
			__func__, LOG_SHM_FILENAME, strerror(-ret));
		return -1;
	}

	log_user = lfb_user(lfb_shm_lfb(&log_shm));

	if (ret == 1)
		lfb_shm_publish(&log_shm);

	return 0;
}

int ubx_log_init(struct ubx_node *nd)
{
	int ret = 0;

	nd->log_data = NULL;

	pthread_mutex_lock(&log_nodes_lock);

	if (log_nodes == 0)
		ret = log_open();

	if (ret == 0) {
		log_nodes++;
		nd->log = ubx_log_shm;
	}

	pthread_mutex_unlock(&log_nodes_lock);
	return ret;
}

void ubx_log_cleanup(struct ubx_node *nd)
{
	/* not joined (failed init, repeated cleanup) */
	if (nd->log != ubx_log_shm)
		return;

	nd->log = NULL;

	pthread_mutex_lock(&log_nodes_lock);

	/* close but don't unlink: other processes may still be logging
	 * into this segment */
	if (--log_nodes == 0) {
		log_user = NULL;
		lfb_shm_close(&log_shm);
	}

	pthread_mutex_unlock(&log_nodes_lock);
}
#endif
