/*
 * A file descriptor event trigger block
 */

#undef UBX_DEBUG

/* CONFIG_PTHREAD_SETNAME, CONFIG_PTHREAD_SETAFFINITY, HAVE_GPIO_V2 and
 * _GNU_SOURCE are defined by the build system (see CMakeLists.txt). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <sys/ioctl.h>

#ifdef HAVE_GPIO_V2
#include <linux/gpio.h>
#endif

#include "ubx.h"
#include "trig_utils.h"
#include "common.h"
#include "rtthread.h"

#define FDTRIG_READ_SIZE_DEF	64
#define FDTRIG_GPIO_EVBUF	16

/* bounds a wakeup on a source that never runs dry, e.g. /dev/zero */
#define FDTRIG_MAX_READS	64

char fdtrig_meta[] =
	"{ doc='file descriptor event trigger',"
	"  description=[[ Steps the active chain once per event on a"
	"                 gpio line, a timerfd or a pollable file. Events"
	"                 pending at wakeup are drained and stepped once. ]],"
	"  realtime=true,"
	"}";

ubx_proto_port_t fdtrig_ports[] = {
	{ .name = "active_chain", .in_type_name = "int", .doc = "switch the active trigger chain" },
	{ .name = "tstats", .out_type_name = "struct ubx_tstat", .doc = "out port for timing statistics" },
	{ .name = "event_cnt", .out_type_name = "uint64_t", .doc = "cumulative count of event wakeups" },
	{ .name = "timeout_cnt", .out_type_name = "uint64_t", .doc = "cumulative count of poll timeouts" },
	{ .name = "coalesced_cnt", .out_type_name = "uint64_t",
	  .doc = "cumulative count of events beyond the first per wakeup (drained, not stepped)" },
	{ .name = "lost_cnt", .out_type_name = "uint64_t",
	  .doc = "gpio: cumulative count of events lost in the kernel buffer (line_seqno gaps)" },
	{ .name = "latency_ns", .out_type_name = "int64_t",
	  .doc = "gpio: CLOCK_MONOTONIC at step start minus the event timestamp [ns]" },
	{ .name = "event_ts_ns", .out_type_name = "uint64_t",
	  .doc = "gpio: CLOCK_MONOTONIC timestamp of the event [ns], written before the chain runs" },
	TRIG_SYNC_PORTS,
	{ 0 },
};

ubx_proto_config_t fdtrig_config[] = {
	{ .name = "source", .type_name = "char", .doc = "event source: gpio, timerfd or path" },
	{ .name = "gpio_chip", .type_name = "char", .doc = "gpio: chip name (gpiochip0) or path (/dev/gpiochip0)" },
	{ .name = "gpio_line", .type_name = "uint32_t", .max = 1, .doc = "gpio: line offset" },
	{ .name = "gpio_edge", .type_name = "char", .doc = "gpio: rising (def), falling or both" },
	{ .name = "period_ns", .type_name = "int64_t", .max = 1, .doc = "timerfd: period [ns], > 0" },
	{ .name = "path", .type_name = "char", .doc = "path: file to poll for POLLIN, reopened on start" },
	{ .name = "read_size", .type_name = "uint32_t", .max = 1, .doc = "path: bytes read and discarded per read (def: 64)" },
	{ .name = "timeout_ns", .type_name = "int64_t", .max = 1, .doc = "poll timeout [ns] since the last wakeup, 0: none (def)" },
	{ .name = "timeout_step", .type_name = "int", .max = 1, .doc = "1: step the active chain on a timeout, 0: only count it (def)" },
	RTTHREAD_CONFIG_ENTRIES("scheduling policy: SCHED_OTHER (default), SCHED_FIFO, SCHED_RR (no SCHED_DEADLINE)"),
	{ .name = "autostop_steps", .type_name = "int64_t", .doc = "if set and > 0, block stops itself after X steps", .max=1 },
	{ .name = "num_chains", .type_name = "int", .max = 1, .doc = "number of trigger chains (def: 1)" },
	{ .name = "tstats_mode", .type_name = "int", .max = 1, .doc = "0: off (def), 1: global only, 2: per block", },
	{ .name = "tstats_profile_path", .type_name = "char", .doc = "directory to write the timing stats file to" },
	{ .name = "tstats_output_rate", .type_name = "double", .max = 1, .doc = "min seconds between tstats port outputs (0: only emit on stop)" },
	{ .name = "tstats_skip_first", .type_name = "int", .max=1, .doc = "skip N steps before acquiring stats" },
	{ .name = "loglevel", .type_name = "int" },
	{ 0 },
};

enum fdtrig_source {
	SRC_GPIO,
	SRC_TIMERFD,
	SRC_PATH,
};

struct fdtrig_inf {
	struct rtthread rt;

	struct ubx_chain *chains;
	int num_chains;
	int actchain;
	int configured;		/* chains configured, also after a self-stop */
	ubx_port_t *p_actchain;
	struct trig_sync sync;

	int64_t autostop_steps;

	enum fdtrig_source source;
	int fd;
	int wake_fd;		/* eventfd, wakes the thread from poll */

	struct itimerspec timer;	/* timerfd */

	char path[PATH_MAX];	/* path */
	uint8_t *buf;
	uint32_t read_size;
	int eof;		/* read returned 0 */

	uint64_t ev_ts;		/* gpio: timestamp of the first event drained */
	uint32_t last_seqno;	/* gpio: line_seqno of the last event */
	int have_seqno;

	struct timespec timeout;
	int has_timeout;
	int timeout_step;

	uint64_t event_cnt;
	uint64_t timeout_cnt;
	uint64_t coalesced_cnt;
	uint64_t lost_cnt;

	ubx_port_t *p_event_cnt;
	ubx_port_t *p_timeout_cnt;
	ubx_port_t *p_coalesced_cnt;
	ubx_port_t *p_lost_cnt;
	ubx_port_t *p_latency;
	ubx_port_t *p_event_ts;
};

#ifdef HAVE_GPIO_V2
static void fdtrig_gpio_seqno(struct fdtrig_inf *inf, uint32_t seqno)
{
	if (inf->have_seqno && seqno != inf->last_seqno + 1)
		inf->lost_cnt += seqno - inf->last_seqno - 1;

	inf->last_seqno = seqno;
	inf->have_seqno = 1;
}
#endif

/*
 * Read the pending events from the source fd, at most
 * FDTRIG_MAX_READS reads. Returns the number of events, or -1 on a
 * read error.
 */
static int64_t fdtrig_drain(ubx_block_t *b, struct fdtrig_inf *inf)
{
	int64_t n = 0;
	ssize_t ret = 0;

	switch (inf->source) {
	case SRC_TIMERFD: {
		/* returns the sum of all expirations */
		uint64_t exp;

		ret = read(inf->fd, &exp, sizeof(exp));
		if (ret == sizeof(exp))
			n = (int64_t)exp;
		break;
	}
	case SRC_PATH:
		for (int i = 0; i < FDTRIG_MAX_READS; i++) {
			ret = read(inf->fd, inf->buf, inf->read_size);
			if (ret <= 0)
				break;
			n++;
			if ((size_t)ret < inf->read_size)
				break;
		}
		if (ret == 0)
			inf->eof = 1;
		break;
#ifdef HAVE_GPIO_V2
	case SRC_GPIO: {
		struct gpio_v2_line_event ev[FDTRIG_GPIO_EVBUF];
		uint64_t lost = inf->lost_cnt;

		for (int i = 0; i < FDTRIG_MAX_READS; i++) {
			ret = read(inf->fd, ev, sizeof(ev));
			if (ret <= 0)
				break;
			if (n == 0)
				inf->ev_ts = ev[0].timestamp_ns;
			for (ssize_t k = 0; k < ret / (ssize_t)sizeof(ev[0]); k++)
				fdtrig_gpio_seqno(inf, ev[k].line_seqno);
			n += ret / (ssize_t)sizeof(ev[0]);
			if ((size_t)ret < sizeof(ev))
				break;
		}

		if (inf->lost_cnt != lost)
			write_uint64(inf->p_lost_cnt, &inf->lost_cnt);
		break;
	}
#endif
	default:
		return -1;
	}

	if (ret < 0 && errno != EAGAIN && errno != EINTR) {
		ubx_err(b, "read failed: %s", strerror(errno));
		return -1;
	}

	return n;
}

static void fdtrig_disarm(struct fdtrig_inf *inf)
{
	const struct itimerspec disarm = { 0 };

	if (inf->source == SRC_TIMERFD)
		timerfd_settime(inf->fd, 0, &disarm, NULL);
}

/* stop from the thread, like autostop_steps: it parks and can be restarted */
static void fdtrig_halt(ubx_block_t *b, struct fdtrig_inf *inf)
{
	fdtrig_disarm(inf);
	rtthread_set_state(&inf->rt, BLOCK_STATE_INACTIVE);
	b->block_state = BLOCK_STATE_INACTIVE;
}

static void fdtrig_wake(ubx_block_t *b, struct fdtrig_inf *inf)
{
	if (eventfd_write(inf->wake_fd, 1) != 0)
		ubx_err(b, "failed to wake thread: %s", strerror(errno));
}

static void fdtrig_flush_stats(ubx_block_t *b, struct fdtrig_inf *inf)
{
	int ret;

	common_output_stats(b, inf->chains, inf->num_chains);
	common_log_stats(b, inf->chains, inf->num_chains);

	ret = common_write_stats(b, inf->chains, inf->num_chains);
	if (ret)
		ubx_err(b, "failed to write tstats to profile_path: %d", ret);

	ubx_info(b, "EVENTS: %" PRIu64 ", coalesced %" PRIu64 ", lost %" PRIu64
		 ", timeouts %" PRIu64, inf->event_cnt, inf->coalesced_cnt,
		 inf->lost_cnt, inf->timeout_cnt);
}

/* step the active chain and handle autostop_steps */
static void fdtrig_step(ubx_block_t *b, struct fdtrig_inf *inf)
{
	int ret;

	common_read_actchain(b, inf->p_actchain, inf->num_chains, &inf->actchain);

	ret = common_trigger(&inf->sync, &inf->chains[inf->actchain]);

	if (ret < 0)
		ubx_err(b, "ubx_chain_trigger failed for chain%i", inf->actchain);

	if (ret != 0 && inf->autostop_steps > 0) {
		if (--inf->autostop_steps == 0) {
			ubx_info(b, "autostop_steps reached 0, stopping block");
			fdtrig_halt(b, inf);
		}
	}
}

static void fdtrig_event(ubx_block_t *b, struct fdtrig_inf *inf, int64_t n)
{
	inf->event_cnt++;
	write_uint64(inf->p_event_cnt, &inf->event_cnt);

	if (n > 1) {
		inf->coalesced_cnt += (uint64_t)(n - 1);
		write_uint64(inf->p_coalesced_cnt, &inf->coalesced_cnt);
	}

	if (inf->source == SRC_GPIO) {
		struct timespec now;
		int64_t lat;

		clock_gettime(CLOCK_MONOTONIC, &now);
		lat = (int64_t)((uint64_t)now.tv_sec * NSEC_PER_SEC +
				(uint64_t)now.tv_nsec - inf->ev_ts);

		write_uint64(inf->p_event_ts, &inf->ev_ts);
		write_int64(inf->p_latency, &lat);
	}

	fdtrig_step(b, inf);
}

static const char *fdtrig_revents_str(short revents, int eof)
{
	if (eof)
		return "EOF";
	if (revents & POLLNVAL)
		return "POLLNVAL";
	if (revents & POLLERR)
		return "POLLERR";
	return "POLLHUP";
}

static void *fdtrig_thread(void *arg)
{
	int ret;
	int64_t n;
	int ran = 0;
	eventfd_t wake;
	ubx_block_t *b = (ubx_block_t *)arg;
	struct fdtrig_inf *inf = (struct fdtrig_inf *)b->private_data;
	struct pollfd pfd[2] = {
		{ .fd = -1, .events = POLLIN },
		{ .fd = inf->wake_fd, .events = POLLIN },
	};

	if (rtthread_setup(b, &inf->rt) != 0)
		goto out;

	/* register now, not on the first port access in the RT loop */
	ubx_rcu_thread_init();

	while (1) {
		pthread_mutex_lock(&inf->rt.mutex);

		/* flush before parking: stop unconfigures the chains */
		if (inf->rt.state != BLOCK_STATE_ACTIVE && !inf->rt.shutdown && ran) {
			fdtrig_flush_stats(b, inf);
			ran = 0;
		}

		while (inf->rt.state != BLOCK_STATE_ACTIVE && !inf->rt.shutdown) {
			rtthread_park(&inf->rt);
			pthread_cond_wait(&inf->rt.active_cond, &inf->rt.mutex);
		}

		if (inf->rt.shutdown) {
			pthread_mutex_unlock(&inf->rt.mutex);
			goto out;
		}

		inf->rt.thread_state = THREAD_ACTIVE;
		pthread_mutex_unlock(&inf->rt.mutex);
		ran = 1;

		/* path reopens on start */
		pfd[0].fd = inf->fd;

		ret = ppoll(pfd, 2, inf->has_timeout ? &inf->timeout : NULL, NULL);

		if (ret < 0) {
			if (errno == EINTR)
				continue;
			ubx_err(b, "poll failed: %s, stopping block", strerror(errno));
			fdtrig_halt(b, inf);
			continue;
		}

		/* stop or cleanup: recheck the state */
		if (pfd[1].revents) {
			eventfd_read(inf->wake_fd, &wake);
			continue;
		}

		if (ret == 0) {
			inf->timeout_cnt++;
			write_uint64(inf->p_timeout_cnt, &inf->timeout_cnt);
			if (inf->timeout_step)
				fdtrig_step(b, inf);
			continue;
		}

		if (pfd[0].revents & POLLIN) {
			n = fdtrig_drain(b, inf);
			if (n < 0) {
				fdtrig_halt(b, inf);
				continue;
			}
			if (n > 0)
				fdtrig_event(b, inf, n);
		}

		/* errors, hangup or EOF would wake us continuously */
		if (pfd[0].revents & (POLLERR | POLLHUP | POLLNVAL) || inf->eof) {
			ubx_err(b, "%s on source fd, stopping block",
				fdtrig_revents_str(pfd[0].revents, inf->eof));
			inf->eof = 0;
			fdtrig_halt(b, inf);
		}
	}

out:
	pthread_mutex_lock(&inf->rt.mutex);
	rtthread_park(&inf->rt);
	pthread_mutex_unlock(&inf->rt.mutex);
	return NULL;
}

/*
 * Copy a char config into buf, NUL-terminated.
 * Returns its length, 0 if unset, or -1 if it does not fit.
 */
static long cfg_getstr(ubx_block_t *b, const char *name, char *buf, size_t size)
{
	long len;
	size_t n;
	const char *s;

	len = cfg_getptr_char(b, name, &s);
	assert(len >= 0);

	if (len == 0)
		return 0;

	n = strnlen(s, len);
	if (n >= size) {
		ubx_err(b, "config %s too long (max %zu)", name, size - 1);
		return -1;
	}

	memcpy(buf, s, n);
	buf[n] = '\0';
	return (long)n;
}

/* int64 [ns] config: 0 if unset, -1 if negative */
static int64_t cfg_get_ns(ubx_block_t *b, const char *name)
{
	long len;
	const int64_t *ns;

	len = cfg_getptr_int64(b, name, &ns);
	assert(len >= 0);

	if (len == 0)
		return 0;

	if (*ns < 0) {
		ubx_err(b, "invalid %s %" PRId64 ", must be >= 0", name, *ns);
		return -1;
	}

	return *ns;
}

static void ns_to_timespec(int64_t ns, struct timespec *ts)
{
	ts->tv_sec = (time_t)(ns / NSEC_PER_SEC);
	ts->tv_nsec = (long)(ns % NSEC_PER_SEC);
}

static int fdtrig_open_timerfd(ubx_block_t *b, struct fdtrig_inf *inf)
{
	int64_t ns = cfg_get_ns(b, "period_ns");

	if (ns <= 0) {
		ubx_err(b, "source timerfd requires a period_ns > 0");
		return EINVALID_CONFIG;
	}

	ns_to_timespec(ns, &inf->timer.it_interval);
	inf->timer.it_value = inf->timer.it_interval;

	inf->fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	if (inf->fd < 0) {
		ubx_err(b, "timerfd_create failed: %s", strerror(errno));
		return EINVALID_CONFIG;
	}

	ubx_info(b, "source timerfd, period %" PRId64 "ns", ns);
	return 0;
}

static int fdtrig_path_open(ubx_block_t *b, struct fdtrig_inf *inf)
{
	inf->fd = open(inf->path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (inf->fd < 0) {
		ubx_err(b, "failed to open %s: %s", inf->path, strerror(errno));
		return EINVALID_CONFIG;
	}
	return 0;
}

static void fdtrig_path_close(struct fdtrig_inf *inf)
{
	if (inf->source == SRC_PATH && inf->fd >= 0) {
		close(inf->fd);
		inf->fd = -1;
	}
}

/* opened in start, here only checked */
static int fdtrig_open_path(ubx_block_t *b, struct fdtrig_inf *inf)
{
	long len;
	int ret;
	const uint32_t *read_size;

	len = cfg_getstr(b, "path", inf->path, sizeof(inf->path));
	if (len < 0)
		return EINVALID_CONFIG;

	if (len == 0) {
		ubx_err(b, "source path requires the 'path' config");
		return EINVALID_CONFIG;
	}

	len = cfg_getptr_uint32(b, "read_size", &read_size);
	assert(len >= 0);

	inf->read_size = (len > 0) ? *read_size : FDTRIG_READ_SIZE_DEF;

	if (inf->read_size == 0) {
		ubx_err(b, "read_size must be > 0");
		return EINVALID_CONFIG;
	}

	inf->buf = malloc(inf->read_size);
	if (inf->buf == NULL) {
		ubx_err(b, "failed to alloc read buffer");
		return EOUTOFMEM;
	}

	ret = fdtrig_path_open(b, inf);
	if (ret != 0)
		return ret;

	fdtrig_path_close(inf);

	ubx_info(b, "source path %s, read_size %" PRIu32, inf->path, inf->read_size);
	return 0;
}

#ifdef HAVE_GPIO_V2
static int fdtrig_open_gpio(ubx_block_t *b, struct fdtrig_inf *inf)
{
	long len;
	int chip_fd, ret, flags;
	char chip[PATH_MAX];
	char chip_path[PATH_MAX + 5];
	const char *edge;
	const uint32_t *line;
	uint64_t edge_flags;
	struct gpio_v2_line_request req;

	len = cfg_getstr(b, "gpio_chip", chip, sizeof(chip));
	if (len < 0)
		return EINVALID_CONFIG;

	if (len == 0) {
		ubx_err(b, "source gpio requires the 'gpio_chip' config");
		return EINVALID_CONFIG;
	}

	snprintf(chip_path, sizeof(chip_path), "%s%s",
		 chip[0] == '/' ? "" : "/dev/", chip);

	len = cfg_getptr_uint32(b, "gpio_line", &line);
	assert(len >= 0);

	if (len == 0) {
		ubx_err(b, "source gpio requires the 'gpio_line' config");
		return EINVALID_CONFIG;
	}

	len = cfg_getptr_char(b, "gpio_edge", &edge);
	assert(len >= 0);

	if (len == 0 || cfg_strmatch(edge, len, "rising")) {
		edge_flags = GPIO_V2_LINE_FLAG_EDGE_RISING;
	} else if (cfg_strmatch(edge, len, "falling")) {
		edge_flags = GPIO_V2_LINE_FLAG_EDGE_FALLING;
	} else if (cfg_strmatch(edge, len, "both")) {
		edge_flags = GPIO_V2_LINE_FLAG_EDGE_RISING | GPIO_V2_LINE_FLAG_EDGE_FALLING;
	} else {
		ubx_err(b, "invalid gpio_edge %.*s, expected rising, falling or both",
			(int)strnlen(edge, len), edge);
		return EINVALID_CONFIG;
	}

	/* no EVENT_CLOCK flag: timestamps are CLOCK_MONOTONIC */
	memset(&req, 0, sizeof(req));
	req.offsets[0] = *line;
	req.num_lines = 1;
	req.config.flags = GPIO_V2_LINE_FLAG_INPUT | edge_flags;
	/* the consumer label is truncated to fit */
	memcpy(req.consumer, b->name,
	       strnlen(b->name, sizeof(req.consumer) - 1));

	chip_fd = open(chip_path, O_RDONLY | O_CLOEXEC);
	if (chip_fd < 0) {
		ubx_err(b, "failed to open %s: %s", chip_path, strerror(errno));
		return EINVALID_CONFIG;
	}

	ret = ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &req);
	if (ret < 0)
		ubx_err(b, "failed to request %s line %" PRIu32 ": %s",
			chip_path, *line, strerror(errno));
	close(chip_fd);

	if (ret < 0)
		return EINVALID_CONFIG;

	inf->fd = req.fd;

	flags = fcntl(inf->fd, F_GETFL);
	if (flags < 0 || fcntl(inf->fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		ubx_err(b, "failed to set O_NONBLOCK: %s", strerror(errno));
		return EINVALID_CONFIG;
	}

	ubx_info(b, "source gpio %s line %" PRIu32 ", edge %s", chip_path, *line,
		 edge_flags == GPIO_V2_LINE_FLAG_EDGE_RISING ? "rising" :
		 edge_flags == GPIO_V2_LINE_FLAG_EDGE_FALLING ? "falling" : "both");
	return 0;
}
#else
static int fdtrig_open_gpio(ubx_block_t *b, struct fdtrig_inf *inf)
{
	(void)inf;
	ubx_err(b, "source gpio unavailable: built without the GPIO v2 uAPI (linux/gpio.h)");
	return EINVALID_CONFIG;
}
#endif

static int fdtrig_handle_config(ubx_block_t *b, struct fdtrig_inf *inf)
{
	long len;
	int ret;
	int64_t timeout_ns;
	const char *source;
	const int64_t *autostop_steps;
	const int *timeout_step;

	len = cfg_getptr_int64(b, "autostop_steps", &autostop_steps);
	assert(len >= 0);
	inf->autostop_steps = (len > 0) ? *autostop_steps : -1;

	timeout_ns = cfg_get_ns(b, "timeout_ns");
	if (timeout_ns < 0)
		return EINVALID_CONFIG;

	if (timeout_ns > 0) {
		ns_to_timespec(timeout_ns, &inf->timeout);
		inf->has_timeout = 1;
	}

	len = cfg_getptr_int(b, "timeout_step", &timeout_step);
	assert(len >= 0);
	inf->timeout_step = (len > 0) ? *timeout_step : 0;

	if (inf->timeout_step != 0 && inf->timeout_step != 1) {
		ubx_err(b, "invalid timeout_step %d, expected 0 or 1", inf->timeout_step);
		return EINVALID_CONFIG;
	}

	if (inf->timeout_step && !inf->has_timeout)
		ubx_warn(b, "timeout_step has no effect without a timeout_ns");

	/* without a period SCHED_DEADLINE has nothing to pace */
	ret = rtthread_handle_config(b, &inf->rt, 0);
	if (ret != 0)
		return ret;

	ubx_info(b, "policy %s, prio %d, stacksize %s",
		 schedpol_tostr(inf->rt.policy), inf->rt.priority, inf->rt.stackbuf);

	len = cfg_getptr_char(b, "source", &source);
	assert(len >= 0);

	if (len == 0) {
		ubx_err(b, "mandatory config 'source' unconfigured");
		return EINVALID_CONFIG;
	} else if (cfg_strmatch(source, len, "gpio")) {
		inf->source = SRC_GPIO;
		return fdtrig_open_gpio(b, inf);
	} else if (cfg_strmatch(source, len, "timerfd")) {
		inf->source = SRC_TIMERFD;
		return fdtrig_open_timerfd(b, inf);
	} else if (cfg_strmatch(source, len, "path")) {
		inf->source = SRC_PATH;
		return fdtrig_open_path(b, inf);
	}

	ubx_err(b, "invalid source %.*s, expected gpio, timerfd or path",
		(int)strnlen(source, len), source);
	return EINVALID_CONFIG;
}

static void fdtrig_close(struct fdtrig_inf *inf)
{
	if (inf->fd >= 0)
		close(inf->fd);
	if (inf->wake_fd >= 0)
		close(inf->wake_fd);
	free(inf->buf);
}

static int fdtrig_init(ubx_block_t *b)
{
	int ret = EOUTOFMEM;
	struct fdtrig_inf *inf;

	b->private_data = calloc(1, sizeof(struct fdtrig_inf));

	if (b->private_data == NULL) {
		ubx_err(b, "failed to alloc");
		return ret;
	}

	inf = (struct fdtrig_inf *)b->private_data;
	inf->fd = -1;

	inf->p_actchain = ubx_port_get(b, "active_chain");
	inf->p_event_cnt = ubx_port_get(b, "event_cnt");
	inf->p_timeout_cnt = ubx_port_get(b, "timeout_cnt");
	inf->p_coalesced_cnt = ubx_port_get(b, "coalesced_cnt");
	inf->p_lost_cnt = ubx_port_get(b, "lost_cnt");
	inf->p_latency = ubx_port_get(b, "latency_ns");
	inf->p_event_ts = ubx_port_get(b, "event_ts_ns");
	assert(inf->p_actchain && inf->p_event_cnt && inf->p_timeout_cnt &&
	       inf->p_coalesced_cnt && inf->p_lost_cnt && inf->p_latency &&
	       inf->p_event_ts);

	inf->wake_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (inf->wake_fd < 0) {
		ubx_err(b, "eventfd failed: %s", strerror(errno));
		ret = EOUTOFMEM;
		goto out_free;
	}

	inf->num_chains = common_init_chains(b, &inf->chains);
	if (inf->num_chains <= 0) {
		ret = EINVALID_CONFIG;
		goto out_free;
	}

	rtthread_init(&inf->rt);

	ret = fdtrig_handle_config(b, inf);
	if (ret != 0)
		goto out_err;

	ret = rtthread_create(b, &inf->rt, fdtrig_thread);
	if (ret != 0)
		goto out_err;

	return 0;

out_err:
	rtthread_destroy(&inf->rt);
	common_cleanup(b, &inf->chains, inf->num_chains);
out_free:
	fdtrig_close(inf);
	free(b->private_data);
	return ret;
}

static int fdtrig_start(ubx_block_t *b)
{
	int ret;
	struct fdtrig_inf *inf = (struct fdtrig_inf *)b->private_data;

	/* after a self-stop the thread may still be flushing the stats */
	if (rtthread_wait_inactive(b, &inf->rt) != 0)
		return EWRONG_STATE;

	/* a self-stop skips the stop hook */
	if (inf->configured) {
		common_unconfig(inf->chains, inf->num_chains);
		inf->configured = 0;
	}

	if (inf->source == SRC_PATH) {
		fdtrig_path_close(inf);
		ret = fdtrig_path_open(b, inf);
		if (ret != 0)
			return ret;
	}

	ret = common_config_chains(b, inf->chains, inf->num_chains);
	if (ret != 0)
		goto out_close;

	inf->configured = 1;
	common_sync_init(b, &inf->sync);

	/* the thread is parked: drop events queued while stopped, gaps
	 * before them are not losses */
	inf->have_seqno = 0;
	if (fdtrig_drain(b, inf) < 0) {
		common_unconfig(inf->chains, inf->num_chains);
		inf->configured = 0;
		ret = EINVALID_CONFIG;
		goto out_close;
	}
	inf->eof = 0;

	if (inf->source == SRC_TIMERFD &&
	    timerfd_settime(inf->fd, 0, &inf->timer, NULL) != 0) {
		ubx_err(b, "timerfd_settime failed: %s", strerror(errno));
		common_unconfig(inf->chains, inf->num_chains);
		inf->configured = 0;
		ret = EINVALID_CONFIG;
		goto out_close;
	}

	rtthread_set_state(&inf->rt, BLOCK_STATE_ACTIVE);
	return 0;

out_close:
	fdtrig_path_close(inf);
	return ret;
}

static void fdtrig_stop(ubx_block_t *b)
{
	struct fdtrig_inf *inf = (struct fdtrig_inf *)b->private_data;

	rtthread_set_state(&inf->rt, BLOCK_STATE_INACTIVE);
	fdtrig_wake(b, inf);
	fdtrig_disarm(inf);

	/* on timeout the thread may still use the chains and fd,
	 * cleanup releases them */
	if (rtthread_wait_inactive(b, &inf->rt) == 0) {
		common_unconfig(inf->chains, inf->num_chains);
		inf->configured = 0;
		fdtrig_path_close(inf);
	}
}

static void fdtrig_cleanup(ubx_block_t *b)
{
	struct fdtrig_inf *inf = (struct fdtrig_inf *)b->private_data;

	rtthread_shutdown(&inf->rt);
	fdtrig_wake(b, inf);
	rtthread_cleanup(b, &inf->rt);

	common_cleanup(b, &inf->chains, inf->num_chains);
	fdtrig_close(inf);
	free(b->private_data);
}

ubx_proto_block_t fdtrig_comp = {
	.name = "ubx/fdtrig",
	.type = BLOCK_TYPE_COMPUTATION,
	.attrs = BLOCK_ATTR_TRIGGER | BLOCK_ATTR_ACTIVE,
	.meta_data = fdtrig_meta,

	.configs = fdtrig_config,
	.ports = fdtrig_ports,

	.init = fdtrig_init,
	.start = fdtrig_start,
	.stop = fdtrig_stop,
	.cleanup = fdtrig_cleanup
};

static int fdtrig_mod_init(ubx_node_t *nd)
{
	return ubx_block_register(nd, &fdtrig_comp);
}

static void fdtrig_mod_cleanup(ubx_node_t *nd)
{
	ubx_block_unregister(nd, "ubx/fdtrig");
}

UBX_MODULE_INIT(fdtrig_mod_init)
UBX_MODULE_CLEANUP(fdtrig_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
