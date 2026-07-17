/*
 * ubx_trace: ftrace trace_marker backend
 *
 * Only built with TRACING=MARKER (see ubx_trace.h).
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "ubx_trace.h"

#define MARKER_MSG_MAXLEN 128

static int marker_fd = -1;

/* the fd is opened at load time so that the trace path performs no
 * syscalls besides the write itself */
static void __attribute__((constructor)) marker_init(void)
{
	marker_fd = open("/sys/kernel/tracing/trace_marker", O_WRONLY);

	if (marker_fd == -1)
		marker_fd = open("/sys/kernel/debug/tracing/trace_marker", O_WRONLY);

	if (marker_fd == -1)
		fprintf(stderr, "ubx_trace: failed to open trace_marker, tracing disabled\n");
}

static void __attribute__((destructor)) marker_cleanup(void)
{
	if (marker_fd != -1)
		close(marker_fd);
}

void ubx_trace_marker(const char *fmt, ...)
{
	int len;
	char buf[MARKER_MSG_MAXLEN];
	va_list ap;

	if (marker_fd == -1)
		return;

	va_start(ap, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if (len <= 0)
		return;

	if ((size_t)len >= sizeof(buf))
		len = sizeof(buf) - 1;

	if (write(marker_fd, buf, len) == -1) {
		/* silently drop, tracing must never disturb the RT path */
	}
}
