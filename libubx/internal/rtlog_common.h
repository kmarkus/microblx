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

#define LOG_BUFFER_DEPTH 10000
#define LOG_SHM_FILENAME "rtlog.logshm"

/* the minimum distance from the wptr that logc_seek_to_oldest will
 * keep when seeking to the oldest log message */
#define LOGC_SEEK_OLDEST_CRUSH_ZONE 100

/* helper to handle atomic read/write of wrap_woff and conveniently
 * deal with wrap and woff */
typedef union {
	struct {
		uint32_t wrap;
		uint32_t off;
	};
	uint64_t wrap_off;
} log_wrap_off_t;

/* log buffer header */
typedef struct log_buf
{
	log_wrap_off_t w;	/* wrap and offset */
	uint8_t data[];
} log_buf_t;

/* log frame */
typedef struct log_frame
{
	uint8_t data;
} log_frame_t;
