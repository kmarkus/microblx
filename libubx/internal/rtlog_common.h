/*
 * rtlog_common.h: client library headers
 *
 * Copyright (C) 2018-2020 Markus Klotzbuecher <mk@mkio.de>
 * Copyright (C) 2019 Hamish Guthrie <hamish.guthrie@kistler.com>
 *
 * SPDX-License-Identifier: MPL-2.0
 */

/*
 * rtlog_common.h - definitions shared by the aggregator side
 * (producer) and the consumer side.
 *
 * The ring itself is lfb (see liblfb/): a lossy broadcast buffer for
 * one producer and any number of independent consumers. rtlog adds
 * only the two things lfb deliberately leaves to the application:
 *
 *  - writer serialization. lfb is single-producer, whereas several
 *    ubx nodes (in one or more processes) log into one buffer, so
 *    writes are serialized by a process-shared robust PI mutex kept
 *    in lfb's user area (see log_user_t and lfb_shm_join).
 *
 *  - the segment lifecycle policy: whoever gets there first creates
 *    and initializes the buffer, everybody else attaches to it.
 */

#include <pthread.h>
#include <stdatomic.h>

#include "lfb.h"

#define LOG_BUFFER_DEPTH 10000
#define LOG_SHM_FILENAME "rtlog.logshm"

/*
 * lfb user area: writers hold this lock across lfb_write.
 *
 * It is robust because a writer may die holding it, which is
 * recoverable here: lfb_write copies the frame into the slot and
 * only then advances the write position with a release store, so a
 * writer dying mid-copy leaves the buffer consistent -- the partial
 * frame sits in a slot that was never published, and the next writer
 * overwrites it. Hence pthread_mutex_consistent() rather than
 * tearing the segment down.
 */
typedef struct log_user {
	pthread_mutex_t wlock;
} log_user_t;

/*
 * opaque frame handle returned by logc_read_frame. Note that since
 * the move to lfb this refers to the reader's *own copy* of the
 * frame (logc_info_t.frame), not to memory inside the ring: lfb
 * validates a frame after copying it out, which is what lets it
 * detect a producer overwriting the slot mid-read.
 */
typedef struct log_frame
{
	uint8_t data;
} log_frame_t;
