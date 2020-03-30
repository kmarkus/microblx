/*
 * rtlog_client.h: client library header
 *
 * Copyright (C) 2018-2020 Markus Klotzbuecher <mk@mkio.de>
 * Copyright (C) 2019 Hamish Guthrie <hamish.guthrie@kistler.com>
 *
 * SPDX-License-Identifier: MPL-2.0
 */

typedef struct logc_info
{
	volatile log_buf_t *buf_ptr;
	log_wrap_off_t r;	/* read wrap and offset */

	uint32_t frame_size;

	int shm_fd;
	int shm_size;
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
