/*
 * rtlog_client: client library for reading log data from shared memory buffer
 *
 * Copyright (C) 2018-2020 Markus Klotzbuecher <mk@mkio.de>
 * Copyright (C) 2019 Hamish Guthrie <hamish.guthrie@kistler.com>
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#undef DEBUG

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "rtlog_client.h"

#ifdef DEBUG
# define DBG(fmt, args...) (fprintf(stderr, "%s: ", __func__), \
			    fprintf(stderr, fmt, ##args),	    \
			    fprintf(stderr, "\n"))
#else
# define DBG(fmt, args...)  do {} while (0)
#endif

/**
 * logc_seek_to_oldest - move the read ptr to the oldest log message
 * that can still be read without being immediately overrun (lfb
 * keeps LFB_CRUSH(depth) frames of headroom for that).
 *
 * @param inf pointer to logc_info_t
 */
void logc_seek_to_oldest(logc_info_t *inf)
{
	const lfb_t *b = lfb_shm_lfb(&inf->shm);

	if (b == NULL)
		return;		/* not open */

	lfb_seek(b, &inf->rd, LFB_OLDEST);
}

/**
 * logc_reset_read - reset the read ptr to the write ptr
 *
 * @param inf pointer to logc_info_t
 */
void logc_reset_read(logc_info_t *inf)
{
	const lfb_t *b = lfb_shm_lfb(&inf->shm);

	if (b == NULL)
		return;		/* not open */

	lfb_seek(b, &inf->rd, LFB_NEWEST);
}

/**
 * logc_init - open shm file and initalize client info
 *
 * @param inf local data
 * @param filename name of shm file created by aggregator block.
 * @param frame_size total frame size (from JSON meta-data). Must
 *        match the frame size the producer created the segment with.
 *
 * @return 0 if successfull, non-zero (errno) in case of failure.
 */
int logc_init(logc_info_t *inf,
	     const char *filename,
	     uint32_t frame_size)
{
	int ret;
	const lfb_t *b;

	memset(inf, 0, sizeof(*inf));

	ret = lfb_shm_open(&inf->shm, filename);

	if (ret != 0) {
		DBG("lfb_shm_open failed: %s", strerror(-ret));
		return -ret;
	}

	b = lfb_shm_lfb(&inf->shm);

	/*
	 * the caller states the frame size out of band, so a mismatch
	 * means it disagrees with the producer about the log message
	 * layout: refuse rather than mis-parse every frame
	 */
	if (b->frame_size != frame_size) {
		DBG("frame size mismatch: segment %u, caller %u",
		    b->frame_size, frame_size);
		lfb_shm_close(&inf->shm);
		return EPROTO;
	}

	inf->frame_size = frame_size;
	inf->frame = malloc(frame_size);

	if (inf->frame == NULL) {
		lfb_shm_close(&inf->shm);
		return ENOMEM;
	}

	logc_reset_read(inf);

	DBG("frame_size: %u, depth: %lu",
	    inf->frame_size, (unsigned long)b->depth);

	return 0;
}

/**
 * logc_close - close shm file
 *
 * @param inf local data
 */
void logc_close(logc_info_t *inf)
{
	lfb_shm_close(&inf->shm);
	free(inf->frame);
	inf->frame = NULL;
}

/**
 * logc_has_data - is new data available?
 *
 * Non-consuming: unlike logc_read_frame this only reports the state
 * and never advances the read cursor or resyncs after an overrun.
 *
 * @param inf
 * @return READ_STATUS
 */
enum READ_STATUS logc_has_data(const logc_info_t *inf)
{
	const lfb_t *b = lfb_shm_lfb(&inf->shm);
	lfb_word_t lag;

	if (b == NULL)
		return ERROR;	/* not open */

	lag = lfb_lag(&inf->rd);

	if (lag == 0)
		return NO_DATA;

	if (lag >= b->depth)
		return OVERRUN;

	return NEW_DATA;
}

/**
 * logc_read_frame - read the next frame if available
 *
 * this is a consuming read, in that the readptr is advanced.
 *
 * The frame is copied into client-owned memory and validated against
 * the write position afterwards, so a frame the producer overwrote
 * while it was being read is never handed out (it is reported as an
 * OVERRUN instead). @frame stays valid until the next call.
 *
 * On OVERRUN the cursor has already been resynced to the oldest
 * surviving frame, so calling again simply continues the stream; an
 * explicit logc_seek_to_oldest is no longer required (but remains
 * harmless).
 *
 * @param inf
 * @param frame outvalue to store the read frame.
 * @return READ_STATUS
 */
enum READ_STATUS logc_read_frame(logc_info_t *inf, volatile log_frame_t **frame)
{
	int ret = lfb_read(&inf->rd, inf->frame);

	if (ret == 1) {
		*frame = (volatile log_frame_t *)inf->frame;
		return NEW_DATA;
	}

	if (ret == 0)
		return NO_DATA;

	return OVERRUN;		/* -EPIPE: frames were lost */
}

/**
 * logc_dataptr_get - get a pointer to the frame payload.
 *
 * @param frame frame for which to calculate the payload ptr
 * @return pointer to the frame payload
 */
void *logc_dataptr_get(volatile log_frame_t *frame)
{
	return (void *)frame;
}

void logc_print_stat(const logc_info_t *inf)
{
	(void)inf;

	DBG("lag: %lu, overruns: %lu",
	    (unsigned long)lfb_lag(&inf->rd),
	    (unsigned long)inf->rd.overruns);
}
