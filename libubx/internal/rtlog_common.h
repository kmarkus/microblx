/*
 * rtlog_common.h: client library headers
 *
 * Copyright (C) 2018-2020 Markus Klotzbuecher <mk@mkio.de>
 * Copyright (C) 2019 Hamish Guthrie <hamish.guthrie@kistler.com>
 *
 * SPDX-License-Identifier: MPL-2.0
 */

/*
 * rtlog_common.h - definitions for both aggregator block (producer)
 * and consumer side.
 *
 * Please note the meaning of read and write offsets:
 *
 * - `woff` is where the aggregator block will write the next frame (or
 *   currently is writing to)
 *
 * - `roff` points to the next frame that a client will read once is
 *   is complete. A frame is complete when the woff has advanced ahead
 *   of roff (and no erroneous conditions such as an overrun occured).
 */

#include <pthread.h>
#include <stdatomic.h>

#define LOG_BUFFER_DEPTH 10000
#define LOG_SHM_FILENAME "rtlog.logshm"

/* the minimum distance from the wptr that logc_seek_to_oldest will
 * keep when seeking to the oldest log message */
#define LOGC_SEEK_OLDEST_CRUSH_ZONE 100

/* helper to conveniently deal with the wrap and woff halves of the
 * atomic wrap_off word */
typedef union {
	struct {
		uint32_t wrap;
		uint32_t off;
	};
	uint64_t wrap_off;
} log_wrap_off_t;

/*
 * w is accessed concurrently from multiple processes, so the 64-bit
 * atomic must be address-free (a libatomic lock-based fallback would
 * only synchronize within one process)
 */
_Static_assert(ATOMIC_LLONG_LOCK_FREE == 2,
	       "rtlog requires lock-free 64-bit atomics");

/* log buffer header */
typedef struct log_buf
{
	pthread_mutex_t wlock;	/* writer lock (process-shared, robust, PI) */
	_Atomic uint64_t w;	/* wrap and offset (a log_wrap_off_t):
				 * stored with release order after the
				 * frame is written, so readers must
				 * load-acquire it before reading frames */
	uint8_t data[];
} log_buf_t;

/* log frame */
typedef struct log_frame
{
	uint8_t data;
} log_frame_t;
