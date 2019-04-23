#include <stdarg.h>

#include "ubx.h"
#include "rtlog.h"

#define CONFIG_SIMPLE_LOGGING 1

/* basic logging function */
void ubx_log(int level, ubx_node_info_t *ni,
	      const char* src, const char* fmt, ...)
{
	va_list args;
	struct ubx_log_msg msg;

	if (level > ni->loglevel)
		return;

	ubx_gettime(&msg.ts);
	msg.level = level;

	strncpy(msg.src, src, BLOCK_NAME_MAXLEN);

	va_start(args, fmt);
	vsnprintf(msg.msg, LOG_MSG_MAXLEN, fmt, args);
	va_end(args);

	ni->log(ni, &msg);

	return;

}

#ifdef CONFIG_SIMPLE_LOGGING
static void ubx_log_simple(struct ubx_node_info* ni, struct ubx_log_msg *msg)
{
	FILE *stream;
	stream = (msg->level <= UBX_LOGLEVEL_WARN) ? stderr : stdout;

	fprintf(stream, "[%li.%li] %s.%s: %s\n",
		msg->ts.sec, msg->ts.nsec / NSEC_PER_USEC,
		ni->name, msg->src, msg->msg);
}

int ubx_log_init(struct ubx_node_info* ni)
{
	ni->log = ubx_log_simple;
	ni->log_data = NULL;
	return 0;
}

void ubx_log_cleanup(struct ubx_node_info* ni)
{
	ni->log = NULL;
}
#endif
