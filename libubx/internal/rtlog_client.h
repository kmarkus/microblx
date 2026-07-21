/*
 * rtlog_client.h: client library header
 *
 * Copyright (C) 2018-2020 Markus Klotzbuecher <mk@mkio.de>
 * Copyright (C) 2019 Hamish Guthrie <hamish.guthrie@kistler.com>
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include "lfb_shm.h"

typedef struct logc_info
{
	lfb_shm_t shm;		/* read-only mapping of the log segment */
	lfb_rd_t rd;		/* private read cursor */

	uint32_t frame_size;

	/*
	 * frame_size bytes owned by this client. lfb_read copies into
	 * it and validates the copy afterwards, so what logc_read_frame
	 * hands out stays stable even if the producer laps us meanwhile.
	 * Valid until the next logc_read_frame on this client.
	 */
	void *frame;
} logc_info_t;

enum READ_STATUS {
	NO_DATA,
	NEW_DATA,
	OVERRUN,
	ERROR
};

/* rtlog_client.c */
void logc_reset_read(logc_info_t *inf);
void logc_seek_to_oldest(logc_info_t *inf);
int logc_init(logc_info_t *inf, const char *filename, uint32_t frame_size);
void logc_close(logc_info_t *inf);
enum READ_STATUS logc_has_data(const logc_info_t *inf);
enum READ_STATUS logc_read_frame(logc_info_t *inf, volatile log_frame_t** frame);
void* logc_dataptr_get(volatile log_frame_t* frame);
void logc_print_stat(const logc_info_t *inf);
