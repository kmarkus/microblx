/*
 * ubx_trace: tracefs access and the ftrace trace_marker backend
 *
 * Not built with TRACING=OFF, the marker backend only with
 * TRACING=MARKER (see ubx_trace.h).
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "ubx_trace.h"

int ubx_tracefs_open(const char *file)
{
	static const char *const roots[] = {
		"/sys/kernel/tracing", "/sys/kernel/debug/tracing",
	};
	char path[128];
	int fd = -1;

	for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]) && fd == -1; i++) {
		snprintf(path, sizeof(path), "%s/%s", roots[i], file);
		fd = open(path, O_WRONLY);
	}

	return fd;
}

#ifdef CONFIG_TRACE_MARKER

#define MARKER_MSG_MAXLEN 128

static int marker_fd = -1;

/* the fd is opened at load time so that the trace path performs no
 * syscalls besides the write itself */
static void __attribute__((constructor)) marker_init(void)
{
	marker_fd = ubx_tracefs_open("trace_marker");

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

#endif /* CONFIG_TRACE_MARKER */
