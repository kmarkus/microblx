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
 * The ring is lfbm (see liblfb/): a lossy broadcast buffer for any
 * number of producers and independent consumers. Several ubx nodes,
 * in one or more processes, log into one buffer without a lock, so a
 * logging thread never waits for another one. rtlog only adds the
 * segment lifecycle policy: whoever gets there first creates the
 * buffer, everybody else attaches to it.
 */

#include "lfbm.h"

#define LOG_BUFFER_DEPTH 8192
#define LOG_SHM_FILENAME "rtlog.logshm"

/* lfb frame_size of a log message */
#define LOG_SLOT_SZ LFBM_SLOT_SZ((uint32_t)sizeof(struct ubx_log_msg))

/*
 * lfb user area. Its size also aligns the slots to the cache line.
 *
 * @dropped: messages no slot could be claimed for
 */
typedef struct log_user {
	_Atomic uint64_t dropped;
	uint8_t pad[24];
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
