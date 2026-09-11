/*
 * A pthread based trigger block
 */

#undef UBX_DEBUG

/* CONFIG_PTHREAD_SETNAME, CONFIG_PTHREAD_SETAFFINITY, and _GNU_SOURCE are
 * defined by the build system when the respective GNU extensions are available
 * (see CMakeLists.txt). */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <inttypes.h>

#include <pthread.h>
#include <limits.h>	/* PTHREAD_STACK_MIN */
#include <sys/syscall.h>
#include <sys/prctl.h>

/* Fall back to a local definition when the toolchain headers don't supply
 * struct sched_attr (older glibc / older kernel headers). Detected by CMake. */
#ifndef HAVE_STRUCT_SCHED_ATTR
struct sched_attr {
	uint32_t size;
	uint32_t sched_policy;
	uint64_t sched_flags;
	int32_t  sched_nice;
	uint32_t sched_priority;
	uint64_t sched_runtime;
	uint64_t sched_deadline;
	uint64_t sched_period;
	uint32_t sched_util_min;
	uint32_t sched_util_max;
};
#endif

#ifndef HAVE_SCHED_FLAG_DL_OVERRUN
#define SCHED_FLAG_DL_OVERRUN	0x4
#endif

/* sched_setattr(2) has no glibc wrapper before 2.41; always go via syscall. */
static int __ubx_sched_setattr(pid_t pid, struct sched_attr *attr, unsigned int flags)
{
	return (int)syscall(SYS_sched_setattr, pid, attr, flags);
}

#include "ubx.h"
#include "ubx_trace.h"
#include "trig_utils.h"
#include "common.h"

#include "types/ptrig_period.h"
#include "types/ptrig_period.h.hexarr"

#include "types/ptrig_deadline.h"
#include "types/ptrig_deadline.h.hexarr"

/* wait 1 second for thread to stop */
#define	THREAD_STOP_TIMEOUT_US	50000
#define	THREAD_STOP_RETRIES	20

/* default sleep_mode=2 busy-wait tail. Sized to cover the worst-case
 * wakeup latency of an RT-tuned ARM SoC; trim it on faster targets. */
#define	PTRIG_BUSY_SLACK_NS_DEF	50000

/* warn when the busy-wait tail exceeds this fraction of the period */
#define	PTRIG_BUSY_SLACK_WARN_RATIO	10

char ptrig_meta[] =
	"{ doc='pthread based trigger',"
	"  realtime=true,"
	"}";

ubx_proto_port_t ptrig_ports[] = {
	{ .name = "active_chain", .in_type_name = "int", .doc = "switch the active trigger chain" },
	{ .name = "tstats", .out_type_name = "struct ubx_tstat", .doc = "out port for timing statistics" },
	{ .name = "shutdown", .in_type_name = "int", .doc = "input port for stopping ptrig" },
	{ .name = "period", .in_type_name = "struct ptrig_period", .doc = "dynamically change the trigger period" },
	{ .name = "period_ns", .in_type_name = "int64_t", .doc = "dynamically change the trigger period [ns]" },
	{ .name = "sched_deadline", .in_type_name = "struct ptrig_deadline",
	  .doc = "update SCHED_DEADLINE parameters at runtime" },
	{ .name = "deadline_throt_cnt", .out_type_name = "uint64_t",
	  .doc = "cumulative SCHED_DEADLINE budget overrun count (requires Linux >= 4.16)" },
	{ .name = "overrun_cnt", .out_type_name = "uint64_t",
	  .doc = "cumulative count of missed trigger deadlines (sleep/busy-wait modes)" },
	{ .name = "latency_ns", .out_type_name = "int64_t",
	  .doc = "trigger latency: how late this trigger fired relative to its deadline grid point [ns] (requires latency_stats=1)" },
	{ 0 },
};

ubx_type_t ptrig_types[] = {
	def_struct_type(struct ptrig_period, &ptrig_period_h),
	def_struct_type(struct ptrig_deadline, &ptrig_deadline_h),
};

def_cfg_getptr_fun(cfg_getptr_ptrig_period, struct ptrig_period);
def_port_accessors(ptrig_period, struct ptrig_period);

def_cfg_getptr_fun(cfg_getptr_ptrig_deadline, struct ptrig_deadline);
def_port_accessors(ptrig_deadline, struct ptrig_deadline);

static void __ptrig_stop(ubx_block_t *b);

ubx_proto_config_t ptrig_config[] = {
	{ .name = "period", .type_name = "struct ptrig_period", .doc = "trigger period in { sec, usec }", },
	{ .name = "period_ns", .type_name = "int64_t", .max = 1, .doc = "trigger period [ns] (mutually exclusive with 'period')", },
	{ .name = "stacksize", .type_name = "size_t", .doc = "stacksize as per pthread_attr_setstacksize(3)" },
	{ .name = "sched_priority", .type_name = "int", .doc = "thread priority (unused with SCHED_DEADLINE)" },
	{ .name = "sched_policy", .type_name = "char", .doc = "scheduling policy: SCHED_OTHER (default), SCHED_FIFO, SCHED_RR, SCHED_DEADLINE (Linux>=3.14)" },
#ifdef CONFIG_PTHREAD_SETAFFINITY
	{ .name = "affinity", .type_name = "int", .doc = "list of CPUs to set the pthread CPU affinity to" },
#endif
	{ .name = "thread_name", .type_name = "char", .doc = "thread name (for dbg), default is block name" },
	{ .name = "autostop_steps", .type_name = "int64_t", .doc = "if set and > 0, block stops itself after X steps", .max=1 },
	{ .name = "num_chains", .type_name = "int", .max = 1, .doc = "number of trigger chains (def: 1)" },

	{ .name = "tstats_mode", .type_name = "int", .max = 1, .doc = "0: off (def), 1: global only, 2: per block", },
	{ .name = "tstats_profile_path", .type_name = "char", .doc = "directory to write the timing stats file to" },
	{ .name = "tstats_output_rate", .type_name = "double", .max = 1, .doc = "min seconds between tstats port outputs (0: only emit on stop)" },
	{ .name = "tstats_skip_first", .type_name = "int", .max=1, .doc = "skip N steps before acquiring stats" },
	{ .name = "sleep_mode", .type_name = "int", .max = 1, .doc = "0: OS sleep (ubx_nanosleep, def), 1: busy-wait (ubx_nanowait), 2: hybrid (sleep, then busy-wait the last busy_slack_ns)",  },
	{ .name = "busy_slack_ns", .type_name = "int64_t", .max = 1, .doc = "sleep_mode=2: duration to busy-wait before the deadline [ns] (def: 50000). Must exceed the platform's worst-case wakeup latency", },
	{ .name = "latency_stats", .type_name = "int", .max = 1, .doc = "1: measure trigger latency (one extra clock read per cycle), emit it on latency_ns and log min/max/avg on stop. 0: off (def)", },
	{ .name = "timerslack_ns", .type_name = "int64_t", .max = 1, .doc = "thread timer slack [ns] (prctl(PR_SET_TIMERSLACK)); 0 (def): leave unchanged. Only affects SCHED_OTHER, where Linux defaults to 50us", },
	{ .name = "loglevel", .type_name = "int" },
	{ .name = "sched_deadline", .type_name = "struct ptrig_deadline", .max = 1,
	  .doc = "SCHED_DEADLINE params { runtime_ns, deadline_ns, period_ns }; "
	         "deadline_ns/period_ns=0 derives from 'period' config" },
	{ 0 },
};


/* used by the thread to report its actual state */
enum thread_state {
	THREAD_INACTIVE,
	THREAD_ACTIVE
};

const char* schedpol_tostr(unsigned int schedpol)
{
	switch(schedpol) {
	case SCHED_OTHER:   return "SCHED_OTHER";
	case SCHED_FIFO:    return "SCHED_FIFO";
	case SCHED_RR:      return "SCHED_RR";
	case SCHED_IDLE:    return "SCHED_IDLE";
	case SCHED_BATCH:   return "SCHED_BATCH";
	case SCHED_DEADLINE: return "SCHED_DEADLINE";
	default:            return "unknown";
	}
}

/**
 * block info
 */
struct ptrig_inf {
	pthread_t tid;
	pthread_attr_t attr;

	uint32_t state;		/* desired state requested by main */
	uint32_t thread_state;	/* actual state reported by thread */
	int shutdown;		/* request thread to exit (for cleanup) */

	pthread_mutex_t mutex;
	pthread_cond_t active_cond;

	uint64_t period_ns;	/* canonical trigger period in nanoseconds */

	struct ubx_chain *chains;
	int num_chains;
	int actchain;

	int64_t autostop_steps;
	int sleep_mode;
	uint64_t busy_slack_ns;	/* sleep_mode=2: busy-wait tail [ns] */
	int64_t timerslack_ns;	/* thread timer slack [ns], 0: leave unchanged */
	int (*sleep_fn)(const struct ubx_timespec *dur, uint64_t slack_ns);

	uint64_t overrun_cnt;	/* cumulative missed trigger deadlines */
	ubx_port_t *p_overrun_cnt;

	int latency_stats;	/* measure trigger latency */
	unsigned int lat_skip;	/* discard this many samples before accumulating */
	ubx_port_t *p_latency;
	uint64_t lat_cnt;	/* trigger latency accumulators [ns] */
	int64_t lat_min;
	int64_t lat_max;
	uint64_t lat_total;

	ubx_port_t *p_actchain;
	ubx_port_t *p_period;
	ubx_port_t *p_period_ns;

	int use_deadline;
	struct sched_attr deadline_attr;
	ubx_port_t *p_deadline;
	ubx_port_t *p_deadline_throt_cnt;
};


static const char *sleep_mode_tostr(int sleep_mode)
{
	switch (sleep_mode) {
	case 0: return "OS sleep (ubx_nanosleep)";
	case 1: return "busy-wait (ubx_nanowait)";
	case 2: return "hybrid (ubx_nanosleep_hybrid)";
	default: return "unknown";
	}
}

/* uniform sleep_fn signature; the slack arg is only used by the hybrid mode */
static int ptrig_sleep(const struct ubx_timespec *dur, uint64_t slack_ns)
{
	(void)slack_ns;
	return ubx_nanosleep(dur);
}

static int ptrig_wait(const struct ubx_timespec *dur, uint64_t slack_ns)
{
	(void)slack_ns;
	return ubx_nanowait(dur);
}

/* TLS overrun counter: incremented by the signal handler, read in the thread loop. */
static __thread volatile sig_atomic_t deadline_overrun_cnt;

static void sigxcpu_handler(int sig)
{
	(void)sig;
	deadline_overrun_cnt++;
}

/*
 * Validate a ptrig_deadline struct and fill a sched_attr.
 * deadline_ns/period_ns == 0 derives from the ptrig period config.
 */
static int ptrig_deadline_make_attr(ubx_block_t *b, const struct ptrig_inf *inf,
				    const struct ptrig_deadline *dl,
				    struct sched_attr *out)
{
	uint64_t runtime_ns, deadline_ns, sched_period_ns;

	runtime_ns      = dl->runtime_ns;
	sched_period_ns = dl->period_ns   ?: inf->period_ns;
	deadline_ns     = dl->deadline_ns ?: sched_period_ns;

	if (runtime_ns < 1024) {
		ubx_err(b, "sched_deadline.runtime_ns %" PRIu64 " below kernel minimum of 1024ns",
			runtime_ns);
		return -1;
	}

	if (runtime_ns > deadline_ns || deadline_ns > sched_period_ns) {
		ubx_err(b, "sched_deadline constraint violated: "
			"runtime_ns (%" PRIu64 ") <= deadline_ns (%" PRIu64 ") <= period_ns (%" PRIu64 ")",
			runtime_ns, deadline_ns, sched_period_ns);
		return -1;
	}

	*out = (struct sched_attr) {
		.size           = sizeof(struct sched_attr),
		.sched_policy   = SCHED_DEADLINE,
		.sched_runtime  = runtime_ns,
		.sched_deadline = deadline_ns,
		.sched_period   = sched_period_ns,
		.sched_flags    = SCHED_FLAG_DL_OVERRUN,
	};
	return 0;
}

/* Called from the ptrig thread to (re-)apply SCHED_DEADLINE params. */
static int ptrig_deadline_apply(ubx_block_t *b, struct ptrig_inf *inf,
				const struct ptrig_deadline *dl)
{
	struct sched_attr sa;

	if (ptrig_deadline_make_attr(b, inf, dl, &sa) != 0)
		return -1;

	if (__ubx_sched_setattr(0, &sa, 0) != 0) {
		ubx_err(b, "sched_setattr failed: %s", strerror(errno));
		return -1;
	}
	inf->deadline_attr = sa;
	return 0;
}

/* thread entry */
void *thread_startup(void *arg)
{
	int ret;
	ubx_block_t *b;
	struct ptrig_inf *inf;
	struct ptrig_period port_period;
	struct ubx_timespec now_ts, remaining;
	uint64_t next = 0;		/* absolute deadline of the next period [ns] */
	uint64_t now_ns;
	uint64_t remaining_ns;
	uint64_t cur_period_ns;		/* current period [ns], may change at runtime */
	int rearm = 1;			/* (re)initialize the deadline on (re)activation */
	uint64_t last_overrun_cnt = 0;	/* last value emitted on the overrun_cnt port */
	sig_atomic_t last_deadline_overrun_cnt = 0;

	b = (ubx_block_t *) arg;
	inf = (struct ptrig_inf *)b->private_data;

	cur_period_ns = inf->period_ns;

	/* timer slack is a per-thread property, so it must be set here */
	if (inf->timerslack_ns > 0) {
		if (prctl(PR_SET_TIMERSLACK, (unsigned long)inf->timerslack_ns) != 0)
			ubx_err(b, "prctl(PR_SET_TIMERSLACK, %" PRId64 ") failed: %s",
				inf->timerslack_ns, strerror(errno));
	}

	if (inf->use_deadline) {
		struct sigaction sa = {
			.sa_handler = sigxcpu_handler,
			.sa_flags   = 0,
		};
		sigset_t sigxcpu_set;
		sigemptyset(&sigxcpu_set);
		sigaddset(&sigxcpu_set, SIGXCPU);

		sigemptyset(&sa.sa_mask);
		if (sigaction(SIGXCPU, &sa, NULL) != 0)
			ubx_err(b, "sigaction(SIGXCPU) failed: %s", strerror(errno));

		/* unblock SIGXCPU on this thread; the creating thread blocked it
		 * so that only the ptrig thread receives overrun signals */
		pthread_sigmask(SIG_UNBLOCK, &sigxcpu_set, NULL);

		if (__ubx_sched_setattr(0, &inf->deadline_attr, 0) != 0) {
			ubx_err(b, "sched_setattr failed: %s", strerror(errno));
			__ptrig_stop(b);
			b->block_state = BLOCK_STATE_INACTIVE;
			goto out;
		}
	}

	while (1) {

		pthread_mutex_lock(&inf->mutex);

		if (inf->state != BLOCK_STATE_ACTIVE && !inf->shutdown) {
			/*
			 * going inactive: flush stats once. This must
			 * happen before setting THREAD_INACTIVE,
			 * since stop() unconfigures the chains after
			 * observing that state.
			 */
			common_output_stats(b, inf->chains, inf->num_chains);
			common_log_stats(b, inf->chains, inf->num_chains);

			ret = common_write_stats(b, inf->chains, inf->num_chains);

			if (ret)
				ubx_err(b, "failed to write tstats to profile_path: %d", ret);

			/* standalone overrun report, next to the tstats.
			 * Logged here rather than in the stop hook because
			 * autostop_steps parks the thread without going
			 * through it. */
			ubx_info(b, "OVERRUNS: %" PRIu64 " missed trigger deadline(s)",
				 inf->overrun_cnt);

			if (inf->lat_cnt > 0)
				ubx_info(b, "LATENCY: cnt %" PRIu64 ", min %" PRId64
					 " ns, max %" PRId64 " ns, avg %" PRIu64 " ns",
					 inf->lat_cnt, inf->lat_min, inf->lat_max,
					 inf->lat_total / inf->lat_cnt);

			if (inf->use_deadline && deadline_overrun_cnt > 0)
				ubx_warn(b, "%u SCHED_DEADLINE budget overrun(s)",
					 (unsigned int)deadline_overrun_cnt);
		}

		while (inf->state != BLOCK_STATE_ACTIVE && !inf->shutdown) {
			inf->thread_state = THREAD_INACTIVE;
			pthread_cond_wait(&inf->active_cond, &inf->mutex);
			/* we slept: rearm the deadline on re-activation so
			 * we don't try to "catch up" the idle interval */
			rearm = 1;
		}

		if (inf->shutdown) {
			pthread_mutex_unlock(&inf->mutex);
			goto out;
		}

		inf->thread_state = THREAD_ACTIVE;
		pthread_mutex_unlock(&inf->mutex);

		if (rearm) {
			/* (re)anchor the absolute deadline grid to now */
			ret = ubx_gettime(&now_ts);
			if (ret) {
				ubx_err(b, "ubx_gettime failed: %s", strerror(errno));
				goto out;
			}
			next = ubx_ts_to_ns(&now_ts);
			rearm = 0;
		}

		common_read_actchain(b, inf->p_actchain, inf->num_chains, &inf->actchain);

		if (inf->use_deadline) {
			struct ptrig_deadline port_dl;
			if (read_ptrig_deadline(inf->p_deadline, &port_dl) > 0)
				ptrig_deadline_apply(b, inf, &port_dl);
		}

		/* The period ports have no effect under SCHED_DEADLINE (the
		 * kernel paces the thread via sched_attr; use the
		 * sched_deadline port to retune), so skip them in that mode. */
		if (!inf->use_deadline) {
			if (read_ptrig_period(inf->p_period, &port_period) > 0)
				cur_period_ns =
					(uint64_t)port_period.sec * NSEC_PER_SEC +
					(uint64_t)port_period.usec * NSEC_PER_USEC;

			int64_t port_period_ns;
			if (read_int64(inf->p_period_ns, &port_period_ns) > 0)
				cur_period_ns = (uint64_t)port_period_ns;
		}

		/*
		 * Trigger latency: how late we are relative to the grid
		 * point we just slept to. 'next' still holds that deadline
		 * here -- it is only advanced further down, after the
		 * trigger. This is the quantity sleep_mode exists to
		 * reduce, and the one overrun_cnt cannot show: overrun_cnt
		 * only counts *whole* periods lost, so on a 1ms period a
		 * consistent 200us lateness registers as zero overruns.
		 *
		 * Meaningless under SCHED_DEADLINE (the kernel paces the
		 * thread and 'next' is unused) and in free-run.
		 */
		if (inf->latency_stats && !inf->use_deadline && cur_period_ns > 0) {
			int64_t lat;

			ret = ubx_gettime(&now_ts);
			if (ret) {
				ubx_err(b, "ubx_gettime failed: %s", strerror(errno));
				goto out;
			}

			lat = (int64_t)(ubx_ts_to_ns(&now_ts) - next);

			write_int64(inf->p_latency, &lat);

			/*
			 * The port carries every sample; the accumulator skips
			 * the first tstats_skip_first, exactly as the tstats do.
			 * Without this the very first cycle after rearm --
			 * where 'next' was just anchored to now, so the
			 * measurement spans the whole startup -- lands in
			 * lat_max and swamps it: at a 500us period it reported
			 * 8.6ms against a true steady-state max of 59us.
			 */
			if (inf->lat_skip > 0) {
				inf->lat_skip--;
				goto lat_done;
			}

			if (inf->lat_cnt == 0 || lat < inf->lat_min)
				inf->lat_min = lat;
			if (inf->lat_cnt == 0 || lat > inf->lat_max)
				inf->lat_max = lat;
			inf->lat_total += (uint64_t)(lat > 0 ? lat : 0);
			inf->lat_cnt++;
lat_done:		;
		}

		if (ubx_chain_trigger(&inf->chains[inf->actchain]) != 0)
			ubx_err(b, "ubx_chain_trigger failed for chain%i", inf->actchain);

		/* check autostop_steps */
		if (inf->autostop_steps > 0) {
			if (--inf->autostop_steps == 0) {
				ubx_info(b, "autostop_steps reached 0, stopping block");

				/* normally one should call
				 * ubx_block_stop, but since we don't
				 * want to wait for the timeout, we
				 * replicate it here: */
				__ptrig_stop(b);
				b->block_state = BLOCK_STATE_INACTIVE;
				continue;
			}
		}

		if (inf->use_deadline) {
			sig_atomic_t cnt = deadline_overrun_cnt;
			if (cnt != last_deadline_overrun_cnt) {
				uint64_t cnt64 = (uint64_t)(unsigned int)cnt;
				ubx_trace_dl_overrun(cnt64);
				ubx_debug(b, "SCHED_DEADLINE budget overrun (total: %" PRIu64 ")", cnt64);
				write_uint64(inf->p_deadline_throt_cnt, &cnt64);
				last_deadline_overrun_cnt = cnt;
			}
			sched_yield();
			continue;
		}

		/*
		 * Sleep until the next absolute deadline. The deadline grid
		 * (next) advances by exactly one period each cycle, regardless
		 * of how long the trigger took or how late we woke up, so
		 * wake-up jitter does not accumulate: there is no drift.
		 *
		 * We sleep for a *relative* duration (next - now). This keeps
		 * both sleep modes identical (ubx_nanosleep / ubx_nanowait both
		 * take a relative duration) and is independent of the
		 * ubx_gettime time source (CLOCK_MONOTONIC or TSC), which an
		 * absolute clock_nanosleep(TIMER_ABSTIME) would not be.
		 */
		if (cur_period_ns == 0)
			continue;	/* no period configured: free-run */

		next += cur_period_ns;

		ret = ubx_gettime(&now_ts);
		if (ret) {
			ubx_err(b, "ubx_gettime failed: %s", strerror(errno));
			goto out;
		}
		now_ns = ubx_ts_to_ns(&now_ts);

		if (now_ns >= next) {
			/*
			 * Deadline already missed (overrun): drop the missed
			 * trigger(s) and realign to the next future grid
			 * point, so recovering load does not cause a burst of
			 * back-to-back triggers. Phase relative to the grid is
			 * preserved; the dropped triggers are reported on the
			 * overrun_cnt port.
			 *
			 * next is strictly in the future afterwards: with
			 * d = now_ns - next >= 0, the new next is
			 * now_ns + cur_period_ns - (d % cur_period_ns).
			 */
			uint64_t missed = (now_ns - next) / cur_period_ns + 1;

			next += missed * cur_period_ns;
			inf->overrun_cnt += missed;
			ubx_trace_overrun(missed, inf->overrun_cnt);

			if (inf->overrun_cnt != last_overrun_cnt) {
				write_uint64(inf->p_overrun_cnt, &inf->overrun_cnt);
				last_overrun_cnt = inf->overrun_cnt;
			}

			ubx_debug(b, "deadline missed, dropped %" PRIu64 " trigger(s)",
				  missed);
		}

		/*
		 * Wait for the deadline. This must happen on the overrun path
		 * too: returning to the top of the loop directly would trigger
		 * immediately, i.e. at an arbitrary phase, and the next
		 * iteration's "next += cur_period_ns" would then discard the
		 * grid point we just realigned to.
		 */
		remaining_ns = next - now_ns;

		remaining.sec = remaining_ns / NSEC_PER_SEC;
		remaining.nsec = remaining_ns % NSEC_PER_SEC;

		ret = inf->sleep_fn(&remaining, inf->busy_slack_ns);
		if (ret) {
			ubx_err(b, "sleep failed: %s", strerror(errno));
			goto out;
		}
	}

 out:
	pthread_mutex_lock(&inf->mutex);
	inf->thread_state = THREAD_INACTIVE;
	pthread_mutex_unlock(&inf->mutex);
	pthread_exit(NULL);
}

/* exact match of a (possibly not NUL-terminated) char config against s */
static int cfg_strmatch(const char *cfg, long len, const char *s)
{
	size_t n = strnlen(cfg, len);
	return n == strlen(s) && strncmp(cfg, s, n) == 0;
}

/* Called from ptrig_handle_config to parse and validate the sched_deadline
 * config and fill inf->deadline_attr. */
static int ptrig_deadline_config(ubx_block_t *b, struct ptrig_inf *inf)
{
	long len;
	const struct ptrig_deadline *dl_cfg;

	len = cfg_getptr_ptrig_deadline(b, "sched_deadline", &dl_cfg);
	assert(len >= 0);

	if (len == 0) {
		ubx_err(b, "SCHED_DEADLINE requires the 'sched_deadline' config");
		return -1;
	}

	if (inf->sleep_mode != 0) {
		ubx_err(b, "sleep_mode=%d is incompatible with SCHED_DEADLINE (must be 0)",
			inf->sleep_mode);
		return -1;
	}

	if (ptrig_deadline_make_attr(b, inf, dl_cfg, &inf->deadline_attr) != 0)
		return -1;

	inf->use_deadline = 1;
	return 0;
}

int ptrig_handle_config(ubx_block_t *b)
{
	long len;
	int ret = EINVALID_CONFIG;
	int pret;
	unsigned int schedpol;
	const int64_t *autostop_steps;
	const char *schedpol_str;
	const size_t *stacksize = NULL;
	struct ptrig_inf *inf = (struct ptrig_inf *)b->private_data;

	/* autostop_steps */
	len = cfg_getptr_int64(b, "autostop_steps", &autostop_steps);
	assert(len >= 0);

	inf->autostop_steps = (len > 0) ? *autostop_steps : -1;

	/* sleep_mode */
	const int *sleep_mode;
	len = cfg_getptr_int(b, "sleep_mode", &sleep_mode);
	assert(len >= 0);
	inf->sleep_mode = (len > 0) ? *sleep_mode : 0;

	if (inf->sleep_mode < 0 || inf->sleep_mode > 2) {
		ubx_err(b, "invalid sleep_mode %d, expected 0 (sleep), 1 (busy) or 2 (hybrid)",
			inf->sleep_mode);
		goto out;
	}

	switch (inf->sleep_mode) {
	case 1: inf->sleep_fn = ptrig_wait; break;
	case 2: inf->sleep_fn = ubx_nanosleep_hybrid; break;
	default: inf->sleep_fn = ptrig_sleep; break;
	}

	/* busy_slack_ns */
	const int64_t *busy_slack_ns;
	len = cfg_getptr_int64(b, "busy_slack_ns", &busy_slack_ns);
	assert(len >= 0);

	if (len > 0 && *busy_slack_ns < 0) {
		ubx_err(b, "invalid busy_slack_ns %" PRId64 ", must be >= 0",
			*busy_slack_ns);
		goto out;
	}

	inf->busy_slack_ns = (len > 0) ?
		(uint64_t)*busy_slack_ns : PTRIG_BUSY_SLACK_NS_DEF;

	if (inf->sleep_mode != 2 && len > 0)
		ubx_warn(b, "busy_slack_ns has no effect with sleep_mode %d",
			 inf->sleep_mode);

	/* timerslack_ns */
	const int64_t *timerslack_ns;
	len = cfg_getptr_int64(b, "timerslack_ns", &timerslack_ns);
	assert(len >= 0);

	if (len > 0 && *timerslack_ns < 0) {
		ubx_err(b, "invalid timerslack_ns %" PRId64 ", must be >= 0",
			*timerslack_ns);
		goto out;
	}

	inf->timerslack_ns = (len > 0) ? *timerslack_ns : 0;

	/* latency_stats */
	const int *latency_stats;
	len = cfg_getptr_int(b, "latency_stats", &latency_stats);
	assert(len >= 0);
	inf->latency_stats = (len > 0) ? *latency_stats : 0;

	/* share tstats_skip_first: same startup transient, same remedy */
	const int *lat_skip_first;
	len = cfg_getptr_int(b, "tstats_skip_first", &lat_skip_first);
	assert(len >= 0);
	inf->lat_skip = (len > 0 && *lat_skip_first > 0) ? (unsigned int)*lat_skip_first : 0;

	if (inf->latency_stats && inf->use_deadline)
		ubx_warn(b, "latency_stats has no effect with SCHED_DEADLINE "
			 "(use deadline_throt_cnt)");

	/* period / period_ns: exactly one of the two must be configured */
	const struct ptrig_period *period;
	const int64_t *period_ns;
	long len_period_ns;

	len = cfg_getptr_ptrig_period(b, "period", &period);
	assert(len >= 0);

	len_period_ns = cfg_getptr_int64(b, "period_ns", &period_ns);
	assert(len_period_ns >= 0);

	if (len > 0 && len_period_ns > 0) {
		ubx_err(b, "configs 'period' and 'period_ns' are mutually exclusive");
		goto out;
	}

	if (len > 0) {
		inf->period_ns = (uint64_t)period->sec * NSEC_PER_SEC +
				 (uint64_t)period->usec * NSEC_PER_USEC;
	} else if (len_period_ns > 0) {
		inf->period_ns = (uint64_t)*period_ns;
	} else {
		ubx_err(b, "mandatory config 'period' or 'period_ns' unconfigured");
		goto out;
	}

	if (inf->sleep_mode == 2 && inf->period_ns > 0) {
		if (inf->busy_slack_ns >= inf->period_ns) {
			ubx_err(b, "busy_slack_ns (%" PRIu64 ") must be less than the period (%" PRIu64 ")",
				inf->busy_slack_ns, inf->period_ns);
			goto out;
		}

		if (inf->busy_slack_ns * PTRIG_BUSY_SLACK_WARN_RATIO > inf->period_ns)
			ubx_warn(b, "busy_slack_ns (%" PRIu64 ") is %" PRIu64 "%% of the period: "
				 "the trigger will busy-wait that share of one CPU",
				 inf->busy_slack_ns,
				 (inf->busy_slack_ns * 100) / inf->period_ns);
	}

	/* stacksize */
	len = cfg_getptr_size_t(b, "stacksize", &stacksize);
	assert(len >= 0);

	if (len > 0) {
		if (*stacksize < (size_t)PTHREAD_STACK_MIN) {
			ubx_err(b, "stacksize (%zd) less than PTHREAD_STACK_MIN (%ld)",
				*stacksize, (long)PTHREAD_STACK_MIN);
			goto out;
		}

		pret = pthread_attr_setstacksize(&inf->attr, *stacksize);
		if (pret) {
			ubx_err(b, "pthread_attr_setstacksize failed: %s",
				strerror(pret));
			goto out;
		}
	}

	char stackbuf[32] = "default";
	if (stacksize != NULL)
		snprintf(stackbuf, sizeof(stackbuf), "%#zx", *stacksize);

	/* schedpolicy */
	len = cfg_getptr_char(b, "sched_policy", &schedpol_str);
	assert(len >= 0);

	if (len > 0) {
		if (cfg_strmatch(schedpol_str, len, "SCHED_OTHER")) {
			schedpol = SCHED_OTHER;
		} else if (cfg_strmatch(schedpol_str, len, "SCHED_FIFO")) {
			schedpol = SCHED_FIFO;
		} else if (cfg_strmatch(schedpol_str, len, "SCHED_RR")) {
			schedpol = SCHED_RR;
		} else if (cfg_strmatch(schedpol_str, len, "SCHED_DEADLINE")) {
			schedpol = SCHED_DEADLINE;
		} else {
			ubx_err(b, "sched_policy config: illegal value %s",
				schedpol_str);
			goto out;
		}
	} else {
		schedpol = SCHED_OTHER;
	}

	if (schedpol == SCHED_DEADLINE) {
		if (ptrig_deadline_config(b, inf) != 0)
			goto out;

		ubx_info(b, "period %" PRIu64 "ns, policy SCHED_DEADLINE, "
			 "runtime %lluns, deadline %lluns, sched_period %lluns, stacksize %s",
			 inf->period_ns,
			 (unsigned long long)inf->deadline_attr.sched_runtime,
			 (unsigned long long)inf->deadline_attr.sched_deadline,
			 (unsigned long long)inf->deadline_attr.sched_period,
			 stackbuf);
	} else {
		const int *prio;
		struct sched_param sched_param;

		if (inf->period_ns == 0)
			ubx_warn(b, "period is 0: trigger will free-run (busy loop) without sleeping");

		if (pthread_attr_setschedpolicy(&inf->attr, schedpol))
			ubx_err(b, "pthread_attr_setschedpolicy failed");

		/* see PTHREAD_ATTR_SETSCHEDPOLICY(3) */
		pret = pthread_attr_setinheritsched(&inf->attr, PTHREAD_EXPLICIT_SCHED);

		if (pret != 0)
			ubx_err(b, "failed to set PTHREAD_EXPLICIT_SCHED: %s",
				strerror(pret));

		/* priority */
		len = cfg_getptr_int(b, "sched_priority", &prio);
		assert(len >= 0);

		sched_param.sched_priority = (len > 0) ? *prio : 0;

		if (((schedpol == SCHED_FIFO || schedpol == SCHED_RR) &&
		     sched_param.sched_priority == 0) ||
		    (schedpol == SCHED_OTHER && sched_param.sched_priority > 0)) {
			ubx_err(b, "invalid sched_priority %d with policy %s",
				sched_param.sched_priority, schedpol_tostr(schedpol));
		}

		pret = pthread_attr_setschedparam(&inf->attr, &sched_param);

		if (pret != 0) {
			ubx_err(b, "failed to set sched_policy.sched_priority to %d: %s",
				sched_param.sched_priority, strerror(pret));
			goto out;
		}

		char slackbuf[64] = "";
		if (inf->sleep_mode == 2)
			snprintf(slackbuf, sizeof(slackbuf), ", busy_slack %" PRIu64 "ns",
				 inf->busy_slack_ns);

		char tslackbuf[64] = "";
		if (inf->timerslack_ns > 0)
			snprintf(tslackbuf, sizeof(tslackbuf), ", timerslack %" PRId64 "ns",
				 inf->timerslack_ns);

		ubx_info(b, "period %" PRIu64 "ns, policy %s, prio %d, stacksize %s, sleep_mode %s%s%s",
			 inf->period_ns,
			 schedpol_tostr(schedpol),
			 sched_param.sched_priority,
			 stackbuf, sleep_mode_tostr(inf->sleep_mode),
			 slackbuf, tslackbuf);
	}

	ret = 0;
out:
	return ret;
}

/* init */
int ptrig_init(ubx_block_t *b)
{
	long len;
	int ret = EOUTOFMEM;
	const char *threadname;
	struct ptrig_inf *inf;

	b->private_data = calloc(1, sizeof(struct ptrig_inf));

	if (b->private_data == NULL) {
		ubx_err(b, "failed to alloc");
		goto out;
	}

	inf = (struct ptrig_inf *)b->private_data;

	inf->p_actchain = ubx_port_get(b, "active_chain");
	assert(inf->p_actchain != NULL);

	inf->p_period = ubx_port_get(b, "period");
	assert(inf->p_period != NULL);

	inf->p_period_ns = ubx_port_get(b, "period_ns");
	assert(inf->p_period_ns != NULL);

	inf->p_deadline = ubx_port_get(b, "sched_deadline");
	assert(inf->p_deadline != NULL);

	inf->p_deadline_throt_cnt = ubx_port_get(b, "deadline_throt_cnt");
	assert(inf->p_deadline_throt_cnt != NULL);

	inf->p_overrun_cnt = ubx_port_get(b, "overrun_cnt");
	inf->p_latency = ubx_port_get(b, "latency_ns");
	assert(inf->p_overrun_cnt != NULL);

	/* initialize chains and add configs */
	inf->num_chains = common_init_chains(b, &inf->chains);

	if (inf->num_chains <= 0)
		goto out_err;

	inf->thread_state = THREAD_INACTIVE;
	inf->state = BLOCK_STATE_INACTIVE;

	pthread_cond_init(&inf->active_cond, NULL);
	pthread_mutex_init(&inf->mutex, NULL);
	pthread_attr_init(&inf->attr);
	pthread_attr_setdetachstate(&inf->attr, PTHREAD_CREATE_JOINABLE);

	ret = ptrig_handle_config(b);
	if (ret != 0)
		goto out_err;

#ifdef CONFIG_PTHREAD_SETAFFINITY
	/* cpu affinity (set on attr so it is validated before thread starts) */
	const int *aff;
	len = cfg_getptr_int(b, "affinity", &aff);
	assert(len>=0);

	if (len > 0 && inf->use_deadline) {
		ubx_warn(b, "affinity with SCHED_DEADLINE requires the affinity mask "
			 "to match a cpuset root domain (see cpuset(7) with "
			 "sched_load_balance=0); sched_setattr will fail with EPERM otherwise.");
	}

	if (len > 0) {
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);

		for (int i=0; i<len; i++) {
			ubx_info(b, "setting affinity to CPU core %i",	aff[i]);
			CPU_SET(aff[i], &cpuset);
		}

		ret = pthread_attr_setaffinity_np(&inf->attr, sizeof(cpu_set_t), &cpuset);

		if (ret != 0) {
			ubx_err(b, "pthread_attr_setaffinity_np failed: %s", strerror(ret));
			ret = -1;
			goto out_err;
		}
	} else {
		ubx_debug(b, "setting no thread affinity");
	}
#endif

	/* block SIGXCPU on this (creating) thread so it is inherited by the new
	 * thread; thread_startup unblocks it again after installing the handler */
	sigset_t oldmask;
	if (inf->use_deadline) {
		sigset_t sigxcpu_set;
		sigemptyset(&sigxcpu_set);
		sigaddset(&sigxcpu_set, SIGXCPU);
		pthread_sigmask(SIG_BLOCK, &sigxcpu_set, &oldmask);
	}

	/* create thread */
	ret = pthread_create(&inf->tid, &inf->attr, thread_startup, b);

	/* restore the creating thread's mask */
	if (inf->use_deadline)
		pthread_sigmask(SIG_SETMASK, &oldmask, NULL);

	if (ret != 0) {
		ubx_err(b, "pthread_create failed: %s", strerror(ret));
		goto out_err;
	}

#ifdef CONFIG_PTHREAD_SETNAME
	/* pthread_setname_np */
	len = cfg_getptr_char(b, "thread_name", &threadname);
	assert(len>=0);

	threadname = (len > 0) ? threadname : b->name;

	if (pthread_setname_np(inf->tid, threadname))
		ubx_err(b, "failed to set thread_name to %s", threadname);
#endif

	/* OK */
	ret = 0;
	goto out;

 out_err:
	common_cleanup(b, &inf->chains, (inf->num_chains > 0) ? inf->num_chains : 0);
	free(b->private_data);
 out:
	return ret;
}

int ptrig_start(ubx_block_t *b)
{
	int ret;
	struct ptrig_inf *inf;

	inf = (struct ptrig_inf *)b->private_data;

	ret = common_config_chains(b, inf->chains, inf->num_chains);

	if (ret != 0)
		goto out;

	pthread_mutex_lock(&inf->mutex);
	inf->state = BLOCK_STATE_ACTIVE;
	pthread_cond_signal(&inf->active_cond);
	pthread_mutex_unlock(&inf->mutex);

	ret = 0;
out:
	return ret;
}

void __ptrig_stop(ubx_block_t *b)
{
	struct ptrig_inf *inf = (struct ptrig_inf *)b->private_data;

	pthread_mutex_lock(&inf->mutex);
	inf->state = BLOCK_STATE_INACTIVE;
	pthread_mutex_unlock(&inf->mutex);
}


void ptrig_stop(ubx_block_t *b)
{
	struct ptrig_inf *inf = (struct ptrig_inf *)b->private_data;

	__ptrig_stop(b);

	/* wait for the thread to park, then release the chain
	 * resources. On timeout skip the unconfig, as the thread may
	 * still be using the chains (cleanup will release them). */
	for (int i=THREAD_STOP_RETRIES; i>=0; i--) {
		uint32_t state;
		pthread_mutex_lock(&inf->mutex);
		state = inf->thread_state;
		pthread_mutex_unlock(&inf->mutex);
		if (state == THREAD_INACTIVE) {
			common_unconfig(inf->chains, inf->num_chains);
			return;
		}
		usleep(THREAD_STOP_TIMEOUT_US);
	}
	ubx_warn(b, "timeout waiting for pthread to stop");
}

void ptrig_cleanup(ubx_block_t *b)
{
	int ret;

	struct ptrig_inf *inf = (struct ptrig_inf *)b->private_data;

	/* request thread shutdown and wake it up. The thread checks
	 * the flag with the mutex held before waiting, so the signal
	 * cannot get lost. */
	pthread_mutex_lock(&inf->mutex);
	inf->shutdown = 1;
	pthread_cond_signal(&inf->active_cond);
	pthread_mutex_unlock(&inf->mutex);

	ret = pthread_join(inf->tid, NULL);
	if (ret != 0)
		ubx_err(b, "pthread_join failed: %s", strerror(ret));

	pthread_mutex_destroy(&inf->mutex);
	pthread_cond_destroy(&inf->active_cond);
	pthread_attr_destroy(&inf->attr);

	/* even though we call ubx_chain_init in start, it is OK to do
	 * this in cleanup only since start calls realloc which will
	 * just resize to the current size */
	common_cleanup(b, &inf->chains, inf->num_chains);
	free(b->private_data);
}

/* put everything together */
ubx_proto_block_t ptrig_comp = {
	.name = "ubx/ptrig",
	.type = BLOCK_TYPE_COMPUTATION,
	.attrs = BLOCK_ATTR_TRIGGER | BLOCK_ATTR_ACTIVE,
	.meta_data = ptrig_meta,

	.configs = ptrig_config,
	.ports = ptrig_ports,

	.init = ptrig_init,
	.start = ptrig_start,
	.stop = ptrig_stop,
	.cleanup = ptrig_cleanup
};

int ptrig_mod_init(ubx_node_t *nd)
{
	int ret;

	for (unsigned int i=0; i<ARRAY_SIZE(ptrig_types); i++) {
		ret = ubx_type_register(nd, &ptrig_types[i]);
		if (ret != 0) {
			ubx_log(UBX_LOGLEVEL_ERR, nd, __func__,
				"failed to register type %s",
				ptrig_types[i].name);
			goto out;
		}
	}

	ret = ubx_block_register(nd, &ptrig_comp);

	if (ret != 0) {
		ubx_log(UBX_LOGLEVEL_ERR, nd, __func__,
			"failed to register ptrig block");
	}
 out:
	return ret;
}

void ptrig_mod_cleanup(ubx_node_t *nd)
{
	for (unsigned int i=0; i<ARRAY_SIZE(ptrig_types); i++)
		ubx_type_unregister(nd, ptrig_types[i].name);

	ubx_block_unregister(nd, "ubx/ptrig");
}

UBX_MODULE_INIT(ptrig_mod_init)
UBX_MODULE_CLEANUP(ptrig_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
