/*
 * ubx_trace: lightweight tracing instrumentation
 *
 * The backend is selected at compile time via the TRACING cmake
 * option. With the default (TRACING=OFF) all trace macros expand to
 * nothing, so there is zero overhead.
 *
 * Backends:
 *
 * CONFIG_TRACE_SDT: static USDT probes (provider "ubx") via
 * <sys/sdt.h>. A probe compiles to a single nop until a consumer
 * (perf, bpftrace, systemtap, ...) attaches to it, so this backend
 * may be left enabled in production builds.
 *
 * CONFIG_TRACE_MARKER: write events to the kernel ftrace buffer via
 * trace_marker, interleaving them with kernel events (sched, irq,
 * ...) recorded e.g. with trace-cmd. Each event costs one write(2)
 * syscall.
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef UBX_TRACE_H
#define UBX_TRACE_H

#if defined(CONFIG_TRACE_SDT)

#include <sys/sdt.h>

#define ubx_trace_chain_begin(chain_id)	DTRACE_PROBE1(ubx, chain_begin, (chain_id))
#define ubx_trace_chain_end(chain_id)	DTRACE_PROBE1(ubx, chain_end, (chain_id))
#define ubx_trace_step_begin(bname)	DTRACE_PROBE1(ubx, step_begin, (bname))
#define ubx_trace_step_end(bname)	DTRACE_PROBE1(ubx, step_end, (bname))
#define ubx_trace_overrun(missed, total) DTRACE_PROBE2(ubx, overrun, (missed), (total))
#define ubx_trace_dl_overrun(total)	DTRACE_PROBE1(ubx, dl_overrun, (total))

#elif defined(CONFIG_TRACE_MARKER)

#include <stdint.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ubx_trace_marker - write a message to the ftrace trace_marker
 *
 * no-op if trace_marker could not be opened (tracefs not mounted or
 * insufficient permissions).
 */
void ubx_trace_marker(const char *fmt, ...)
	__attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif

#define ubx_trace_chain_begin(chain_id)	ubx_trace_marker("ubx:chain_begin: %s", (chain_id))
#define ubx_trace_chain_end(chain_id)	ubx_trace_marker("ubx:chain_end: %s", (chain_id))
#define ubx_trace_step_begin(bname)	ubx_trace_marker("ubx:step_begin: %s", (bname))
#define ubx_trace_step_end(bname)	ubx_trace_marker("ubx:step_end: %s", (bname))
#define ubx_trace_overrun(missed, total) \
	ubx_trace_marker("ubx:overrun: missed %" PRIu64 " total %" PRIu64, \
			 (uint64_t)(missed), (uint64_t)(total))
#define ubx_trace_dl_overrun(total) \
	ubx_trace_marker("ubx:dl_overrun: total %" PRIu64, (uint64_t)(total))

#else /* tracing disabled */

#define ubx_trace_chain_begin(chain_id)	do {} while (0)
#define ubx_trace_chain_end(chain_id)	do {} while (0)
#define ubx_trace_step_begin(bname)	do {} while (0)
#define ubx_trace_step_end(bname)	do {} while (0)
#define ubx_trace_overrun(missed, total) do {} while (0)
#define ubx_trace_dl_overrun(total)	do {} while (0)

#endif

#endif /* UBX_TRACE_H */
