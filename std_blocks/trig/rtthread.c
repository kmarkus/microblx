/*
 * RT thread handling shared by the threaded trigger blocks
 */

#undef UBX_DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <time.h>
#include <sys/syscall.h>

#include "ubx.h"
#include "rtthread.h"

/* wait 1 second for thread to stop */
#define	THREAD_STOP_TIMEOUT_NS	1000000000L

const char *schedpol_tostr(unsigned int schedpol)
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

/* exact match of a (possibly not NUL-terminated) char config against s */
int cfg_strmatch(const char *cfg, long len, const char *s)
{
	size_t n = strnlen(cfg, len);
	return n == strlen(s) && strncmp(cfg, s, n) == 0;
}

void rtthread_init(struct rtthread *rt)
{
	rt->thread_state = THREAD_INACTIVE;
	rt->state = BLOCK_STATE_INACTIVE;

	pthread_condattr_t ca;

	pthread_cond_init(&rt->active_cond, NULL);
	pthread_condattr_init(&ca);
	pthread_condattr_setclock(&ca, CLOCK_MONOTONIC);
	pthread_cond_init(&rt->park_cond, &ca);
	pthread_condattr_destroy(&ca);
	pthread_mutex_init(&rt->mutex, NULL);
	pthread_attr_init(&rt->attr);
	pthread_attr_setdetachstate(&rt->attr, PTHREAD_CREATE_JOINABLE);
}

/* cpuset and affinity. Returns -1 on error */
static int rtthread_cpu_config(ubx_block_t *b, struct rtthread *rt)
{
	long len;
	int ret;
	const char *cpuset_cfg;

	len = cfg_getptr_char(b, "cpuset", &cpuset_cfg);
	assert(len >= 0);

	if (len > 0) {
		int n = (int)strnlen(cpuset_cfg, len);

		ret = snprintf(rt->cpuset_tasks, sizeof(rt->cpuset_tasks), "%s%.*s/tasks",
			       cpuset_cfg[0] == '/' ? "" : CPUSET_ROOT "/", n, cpuset_cfg);
		if (ret < 0 || (size_t)ret >= sizeof(rt->cpuset_tasks)) {
			ubx_err(b, "cpuset path too long: %.*s", n, cpuset_cfg);
			return -1;
		}
		ubx_info(b, "joining cpuset %s", rt->cpuset_tasks);
	}

#ifdef CONFIG_PTHREAD_SETAFFINITY
	/* cpu affinity: set on attr so it is validated before the thread
	 * starts, or with a cpuset by the thread after joining it */
	const int *aff;
	len = cfg_getptr_int(b, "affinity", &aff);
	assert(len>=0);

	if (len > 0 && rt->policy == SCHED_DEADLINE) {
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

		if (rt->cpuset_tasks[0] != '\0') {
			rt->affinity = cpuset;
			rt->thread_affinity = 1;
			ret = 0;
		} else {
			ret = pthread_attr_setaffinity_np(&rt->attr, sizeof(cpu_set_t), &cpuset);
		}

		if (ret != 0) {
			ubx_err(b, "pthread_attr_setaffinity_np failed: %s", strerror(ret));
			return -1;
		}
	} else {
		ubx_debug(b, "setting no thread affinity");
	}
#endif
	return 0;
}

/**
 * rtthread_handle_config - parse the RTTHREAD_CONFIG_ENTRIES
 *
 * With SCHED_DEADLINE only rt->policy is set, the attr is left to
 * the caller. Without allow_deadline, SCHED_DEADLINE is rejected.
 *
 * @return 0, EINVALID_CONFIG or -1 (cpuset/affinity)
 */
int rtthread_handle_config(ubx_block_t *b, struct rtthread *rt, int allow_deadline)
{
	long len;
	int pret;
	const char *schedpol_str;
	const size_t *stacksize = NULL;

	/* stacksize */
	len = cfg_getptr_size_t(b, "stacksize", &stacksize);
	assert(len >= 0);

	if (len > 0) {
		if (*stacksize < (size_t)PTHREAD_STACK_MIN) {
			ubx_err(b, "stacksize (%zd) less than PTHREAD_STACK_MIN (%ld)",
				*stacksize, (long)PTHREAD_STACK_MIN);
			return EINVALID_CONFIG;
		}

		pret = pthread_attr_setstacksize(&rt->attr, *stacksize);
		if (pret) {
			ubx_err(b, "pthread_attr_setstacksize failed: %s",
				strerror(pret));
			return EINVALID_CONFIG;
		}
	}

	strcpy(rt->stackbuf, "default");
	if (stacksize != NULL)
		snprintf(rt->stackbuf, sizeof(rt->stackbuf), "%#zx", *stacksize);

	/* schedpolicy */
	len = cfg_getptr_char(b, "sched_policy", &schedpol_str);
	assert(len >= 0);

	if (len > 0) {
		if (cfg_strmatch(schedpol_str, len, "SCHED_OTHER")) {
			rt->policy = SCHED_OTHER;
		} else if (cfg_strmatch(schedpol_str, len, "SCHED_FIFO")) {
			rt->policy = SCHED_FIFO;
		} else if (cfg_strmatch(schedpol_str, len, "SCHED_RR")) {
			rt->policy = SCHED_RR;
		} else if (cfg_strmatch(schedpol_str, len, "SCHED_DEADLINE")) {
			rt->policy = SCHED_DEADLINE;
		} else {
			ubx_err(b, "sched_policy config: illegal value %s",
				schedpol_str);
			return EINVALID_CONFIG;
		}
	} else {
		rt->policy = SCHED_OTHER;
	}

	if (rt->policy == SCHED_DEADLINE && !allow_deadline) {
		ubx_err(b, "SCHED_DEADLINE is not supported by this block");
		return EINVALID_CONFIG;
	}

	if (rt->policy != SCHED_DEADLINE) {
		const int *prio;
		struct sched_param sched_param;

		if (pthread_attr_setschedpolicy(&rt->attr, rt->policy))
			ubx_err(b, "pthread_attr_setschedpolicy failed");

		/* see PTHREAD_ATTR_SETSCHEDPOLICY(3) */
		pret = pthread_attr_setinheritsched(&rt->attr, PTHREAD_EXPLICIT_SCHED);

		if (pret != 0)
			ubx_err(b, "failed to set PTHREAD_EXPLICIT_SCHED: %s",
				strerror(pret));

		/* priority */
		len = cfg_getptr_int(b, "sched_priority", &prio);
		assert(len >= 0);

		sched_param.sched_priority = (len > 0) ? *prio : 0;

		if (((rt->policy == SCHED_FIFO || rt->policy == SCHED_RR) &&
		     sched_param.sched_priority == 0) ||
		    (rt->policy == SCHED_OTHER && sched_param.sched_priority > 0)) {
			ubx_err(b, "invalid sched_priority %d with policy %s",
				sched_param.sched_priority, schedpol_tostr(rt->policy));
		}

		pret = pthread_attr_setschedparam(&rt->attr, &sched_param);

		if (pret != 0) {
			ubx_err(b, "failed to set sched_policy.sched_priority to %d: %s",
				sched_param.sched_priority, strerror(pret));
			return EINVALID_CONFIG;
		}
		rt->priority = sched_param.sched_priority;
	}

	return rtthread_cpu_config(b, rt);
}

/**
 * rtthread_create - create the thread, name it and, with a cpuset,
 * wait for it to join, so that init fails rather than run outside of
 * it. fn must call rtthread_setup first.
 */
int rtthread_create(ubx_block_t *b, struct rtthread *rt, void *(*fn)(void *))
{
	int ret;

	ret = pthread_create(&rt->tid, &rt->attr, fn, b);

	if (ret != 0) {
		ubx_err(b, "pthread_create failed: %s", strerror(ret));
		return ret;
	}

#ifdef CONFIG_PTHREAD_SETNAME
	long len;
	const char *threadname;

	len = cfg_getptr_char(b, "thread_name", &threadname);
	assert(len>=0);

	threadname = (len > 0) ? threadname : b->name;

	if (pthread_setname_np(rt->tid, threadname))
		ubx_err(b, "failed to set thread_name to %s", threadname);
#endif

	if (rt->cpuset_tasks[0] != '\0') {
		pthread_mutex_lock(&rt->mutex);
		while (!rt->setup_done)
			pthread_cond_wait(&rt->active_cond, &rt->mutex);
		ret = rt->setup_ret;
		pthread_mutex_unlock(&rt->mutex);

		if (ret != 0)
			pthread_join(rt->tid, NULL);
	}

	return ret;
}

/*
 * Join the cpuset and set the affinity from the thread itself. A thread
 * cannot be created with an affinity outside its creator's cpuset, and
 * joining a cpuset resets the affinity to the cpuset's CPUs, so both
 * happen here, in that order.
 */
static int thread_setup(ubx_block_t *b, struct rtthread *rt)
{
	int fd, ret;

	if (rt->cpuset_tasks[0] == '\0')
		return 0;

	fd = open(rt->cpuset_tasks, O_WRONLY);
	if (fd < 0) {
		ubx_err(b, "failed to open %s: %s", rt->cpuset_tasks, strerror(errno));
		return -1;
	}

	ret = dprintf(fd, "%ld\n", (long)syscall(SYS_gettid));
	if (ret < 0)
		ubx_err(b, "failed to join cpuset %s: %s", rt->cpuset_tasks, strerror(errno));
	close(fd);

	if (ret < 0)
		return -1;

#ifdef CONFIG_PTHREAD_SETAFFINITY
	if (rt->thread_affinity) {
		ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &rt->affinity);
		if (ret != 0) {
			ubx_err(b, "pthread_setaffinity_np failed: %s", strerror(ret));
			return -1;
		}
	}
#endif
	return 0;
}

/**
 * rtthread_setup - run at thread start: join the cpuset and report
 * the result to rtthread_create, which waits for it.
 * @return 0 if OK, the thread must exit otherwise
 */
int rtthread_setup(ubx_block_t *b, struct rtthread *rt)
{
	int ret = thread_setup(b, rt);

	pthread_mutex_lock(&rt->mutex);
	rt->setup_ret = ret;
	rt->setup_done = 1;
	pthread_cond_broadcast(&rt->active_cond);
	pthread_mutex_unlock(&rt->mutex);

	return ret;
}

/* request a state; only activation wakes the thread */
void rtthread_set_state(struct rtthread *rt, uint32_t state)
{
	pthread_mutex_lock(&rt->mutex);
	rt->state = state;
	if (state == BLOCK_STATE_ACTIVE)
		pthread_cond_signal(&rt->active_cond);
	pthread_mutex_unlock(&rt->mutex);
}

/**
 * rtthread_wait_inactive - wait for the thread to park
 * @return 0 if parked, -1 on timeout (the thread may still use the
 * chains)
 */
int rtthread_wait_inactive(ubx_block_t *b, struct rtthread *rt)
{
	int ret = 0;
	struct timespec deadline;

	clock_gettime(CLOCK_MONOTONIC, &deadline);
	deadline.tv_nsec += THREAD_STOP_TIMEOUT_NS;
	deadline.tv_sec += deadline.tv_nsec / 1000000000L;
	deadline.tv_nsec %= 1000000000L;

	pthread_mutex_lock(&rt->mutex);
	while (rt->thread_state != THREAD_INACTIVE && ret == 0)
		ret = pthread_cond_timedwait(&rt->park_cond, &rt->mutex, &deadline);
	if (rt->thread_state == THREAD_INACTIVE)
		ret = 0;
	pthread_mutex_unlock(&rt->mutex);

	if (ret != 0) {
		ubx_warn(b, "timeout waiting for pthread to stop");
		return -1;
	}
	return 0;
}

/* report the thread parked or exited, with rt->mutex held */
void rtthread_park(struct rtthread *rt)
{
	rt->thread_state = THREAD_INACTIVE;
	pthread_cond_broadcast(&rt->park_cond);
}

/* request thread shutdown and wake it up. The thread checks the flag
 * with the mutex held before waiting, so the signal cannot get lost. */
void rtthread_shutdown(struct rtthread *rt)
{
	pthread_mutex_lock(&rt->mutex);
	rt->shutdown = 1;
	pthread_cond_signal(&rt->active_cond);
	pthread_mutex_unlock(&rt->mutex);
}

/* join and release the thread resources, after rtthread_shutdown */
void rtthread_cleanup(ubx_block_t *b, struct rtthread *rt)
{
	int ret;

	ret = pthread_join(rt->tid, NULL);
	if (ret != 0)
		ubx_err(b, "pthread_join failed: %s", strerror(ret));

	rtthread_destroy(rt);
}

/* release the resources of rtthread_init */
void rtthread_destroy(struct rtthread *rt)
{
	pthread_mutex_destroy(&rt->mutex);
	pthread_cond_destroy(&rt->active_cond);
	pthread_cond_destroy(&rt->park_cond);
	pthread_attr_destroy(&rt->attr);
}
