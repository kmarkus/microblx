/*
 * Support for real-time logging
 */

#ifndef _UBX_RTLOG_H
#define _UBX_RTLOG_H

#define UBX_LOGLEVEL_DEFAULT		UBX_LOGLEVEL_INFO

enum {
	UBX_LOGLEVEL_EMERG = 0,		/* system unusable */
	UBX_LOGLEVEL_ALERT = 1,		/* immediate action required */
	UBX_LOGLEVEL_CRIT = 2,		/* critical */
	UBX_LOGLEVEL_ERR = 3,		/* error */
	UBX_LOGLEVEL_WARN = 4,	 	/* warning conditions */
	UBX_LOGLEVEL_NOTICE = 5, 	/* normal but significant */
	UBX_LOGLEVEL_INFO = 6,		/* info msg */
	UBX_LOGLEVEL_DEBUG = 7		/* debug messages */
};

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

#define ubx_block_log(level, b, fmt, ...)				\
	if (b->loglevel && level <= *b->loglevel)					\
		__ubx_log(b->ni, level, b->name, fmt, ##__VA_ARGS__);	\

/* hooks for setting up logging infrastructure */
int ubx_log_init(ubx_node_info_t *ni);
void ubx_log_cleanup(ubx_node_info_t *ni);

/* generic, low-level logging function. blocks should prefer the
 * standard ubx_* functions */
void ubx_log(int level, ubx_node_info_t *ni, const char* src, const char* fmt, ...);

#endif /* _UBX_RTLOG_H */
