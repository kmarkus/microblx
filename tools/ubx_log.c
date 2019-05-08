/*
 * ubx_log: microblx logging client
 *
 * Copyright (C) 2019 Hamish Guthrie <hamish.guthrie@kistler.com>
 *
 * SPDX-License-Identifier: MPL-2.0
 */
#include <stdio.h>
#include <unistd.h>

#include "ubx.h"
#include "rtlog_client.h"

const char *loglevel_str[] = {
	"EMERG", "ALERT", "CRIT", "ERROR",
	"WARN", "NOTICE", "INFO", "DEBUG"
};

void log_data(logc_info_t *inf)
{
	struct ubx_log_msg *msg;
	int ret;
	FILE *stream;
	const char *level_str;

	ret = logc_read_frame(inf, (volatile log_frame_t **)&msg);
	switch (ret) {
	case NO_DATA:
		break;

	case NEW_DATA:
		stream = (msg->level <= UBX_LOGLEVEL_WARN) ? stderr : stdout;
		level_str = (msg->level > UBX_LOGLEVEL_DEBUG ||
			     msg->level < UBX_LOGLEVEL_EMERG) ?
			"INVALID" : loglevel_str[msg->level];

		fprintf(stream, "[%li.%06li] %s %s: %s\n",
			msg->ts.sec, msg->ts.nsec / NSEC_PER_USEC,
			level_str, msg->src, msg->msg);

		break;
	}
}

int main(void)
{
	logc_info_t inf;
	int ret;
	int frames = 0, overrun = 0;

	ret = logc_init(&inf, LOG_SHM_FILENAME, sizeof(struct ubx_log_msg));
	if (ret) {
		fprintf(stderr, "failed to initialsise logging library\n");
		return 1;
	}

	while (1) {
		ret = logc_has_data(&inf);
		switch (ret) {
		case NO_DATA:
			usleep(100000);
			continue;

		case NEW_DATA:
			log_data(&inf);
			frames++;
			break;

		case OVERRUN:
			fprintf(stderr, "OVERRUN - reset read side\n");
			overrun++;
			logc_reset_read(&inf);
			break;

		case ERROR:
			fprintf(stderr, "ERROR checking for data\n");
			logc_reset_read(&inf);
			usleep(100000);
			break;
		}
	}

	logc_close(&inf);

	return 0;
}
