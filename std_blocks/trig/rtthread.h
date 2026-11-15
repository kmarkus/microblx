/*
 * RT thread handling shared by the threaded trigger blocks
 */

#ifndef _TRIG_RTTHREAD_H
#define _TRIG_RTTHREAD_H

#include <pthread.h>
#include <limits.h>	/* PATH_MAX */
#include <stdint.h>

#include "ubx.h"

/* where a relative cpuset config resolves to */
#define CPUSET_ROOT		"/sys/fs/cgroup/cpuset"

#ifdef CONFIG_PTHREAD_SETAFFINITY
#define RTTHREAD_AFFINITY_ENTRY						\
	{ .name = "affinity", .type_name = "int", .doc = "list of CPUs to set the pthread CPU affinity to" },
#else
#define RTTHREAD_AFFINITY_ENTRY
#endif

/* thread configs, shared by the ptrig and fdtrig config arrays */
#define RTTHREAD_CONFIG_ENTRIES(policy_doc)				\
	{ .name = "stacksize", .type_name = "size_t", .doc = "stacksize as per pthread_attr_setstacksize(3)" }, \
	{ .name = "sched_priority", .type_name = "int", .doc = "thread priority (unused with SCHED_DEADLINE)" }, \
	{ .name = "sched_policy", .type_name = "char", .doc = policy_doc }, \
	RTTHREAD_AFFINITY_ENTRY						\
	{ .name = "cpuset", .type_name = "char", .doc = "cgroup v1 cpuset the thread joins on creation: a name under " CPUSET_ROOT " or an absolute path" }, \
	{ .name = "thread_name", .type_name = "char", .doc = "thread name (for dbg), default is block name" }

/* used by the thread to report its actual state */
enum thread_state {
	THREAD_INACTIVE,
	THREAD_ACTIVE
};

/**
 * struct rtthread - a block's trigger thread
 * @state: desired state requested by main
 * @thread_state: actual state reported by the thread
 * @shutdown: request the thread to exit (for cleanup)
 * @policy: configured sched_policy
 * @priority: configured sched_priority (not with SCHED_DEADLINE)
 * @stackbuf: configured stacksize as string, for logging
 * @cpuset_tasks: tasks file of the cpuset to join, "" if none
 * @setup_done: rtthread_setup has run
 * @setup_ret: its result
 */
struct rtthread {
	pthread_t tid;
	pthread_attr_t attr;

	uint32_t state;
	uint32_t thread_state;
	int shutdown;

	pthread_mutex_t mutex;
	pthread_cond_t active_cond;
	pthread_cond_t park_cond;	/* signals THREAD_INACTIVE */

	int policy;
	int priority;
	char stackbuf[32];

	char cpuset_tasks[PATH_MAX];
#ifdef CONFIG_PTHREAD_SETAFFINITY
	int thread_affinity;	/* with cpuset: thread sets the affinity itself */
	cpu_set_t affinity;
#endif
	int setup_done;
	int setup_ret;
};

const char *schedpol_tostr(unsigned int schedpol);
int cfg_strmatch(const char *cfg, long len, const char *s);

void rtthread_init(struct rtthread *rt);
int rtthread_handle_config(ubx_block_t *b, struct rtthread *rt, int allow_deadline);
int rtthread_create(ubx_block_t *b, struct rtthread *rt, void *(*fn)(void *));
int rtthread_setup(ubx_block_t *b, struct rtthread *rt);
void rtthread_set_state(struct rtthread *rt, uint32_t state);
int rtthread_wait_inactive(ubx_block_t *b, struct rtthread *rt);
void rtthread_park(struct rtthread *rt);
void rtthread_shutdown(struct rtthread *rt);
void rtthread_cleanup(ubx_block_t *b, struct rtthread *rt);
void rtthread_destroy(struct rtthread *rt);

#endif /* _TRIG_RTTHREAD_H */
