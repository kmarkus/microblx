/*
 * Support for real-time logging
 *
 * Copyright (C) 2014-2020 Markus Klotzbuecher <mk@mkio.de>
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef _UBX_RTLOG_H
#define _UBX_RTLOG_H

#define UBX_LOGLEVEL_DEFAULT		UBX_LOGLEVEL_INFO

#define ubx_log(level, nd, src, fmt, ...)			\
do {								\
	if (level <= (nd)->loglevel)				\
		__ubx_log(level, nd, src, fmt, ##__VA_ARGS__);	\
} while (0)

#ifdef UBX_DEBUG
# define ubx_debug(b, fmt, ...) ubx_block_log(UBX_LOGLEVEL_DEBUG, b, fmt, ##__VA_ARGS__)
#else
# define ubx_debug(b, fmt, ...) do {} while (0)
#endif

/* standard block logging functions */
#define ubx_emerg(b, fmt, ...)	ubx_block_log(UBX_LOGLEVEL_EMERG, b, fmt, ##__VA_ARGS__)
#define ubx_alert(b, fmt, ...)	ubx_block_log(UBX_LOGLEVEL_ALERT, b, fmt, ##__VA_ARGS__)
#define ubx_crit(b, fmt, ...)	ubx_block_log(UBX_LOGLEVEL_CRIT, b, fmt, ##__VA_ARGS__)
#define ubx_err(b, fmt, ...)	ubx_block_log(UBX_LOGLEVEL_ERR, b, fmt, ##__VA_ARGS__)
#define ubx_warn(b, fmt, ...)	ubx_block_log(UBX_LOGLEVEL_WARN, b, fmt, ##__VA_ARGS__)
#define ubx_notice(b, fmt, ...) ubx_block_log(UBX_LOGLEVEL_NOTICE, b, fmt, ##__VA_ARGS__)
#define ubx_info(b, fmt, ...)	ubx_block_log(UBX_LOGLEVEL_INFO, b, fmt, ##__VA_ARGS__)

#define ubx_block_log(level, b, fmt, ...)					\
do {										\
	if (b->loglevel) {							\
		if (level <= *b->loglevel) {					\
			__ubx_log(level, b->nd, b->name, fmt, ##__VA_ARGS__);	\
		}								\
	} else {								\
		if (level <= b->nd->loglevel) {					\
			__ubx_log(level, b->nd, b->name, fmt, ##__VA_ARGS__);	\
		}								\
	}									\
} while (0)

/* hooks for setting up logging infrastructure */
int ubx_log_init(ubx_node_t *nd);
void ubx_log_cleanup(ubx_node_t *nd);

/*
 * generic, low-level logging function. blocks should prefer the
 * standard ubx_* functions
 */
void __ubx_log(const int level, const ubx_node_t *nd, const char *src, const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));

#endif /* _UBX_RTLOG_H */
