/*
 * A pthread based trigger block
 */

#undef UBX_DEBUG

#define CONFIG_PTHREAD_SETNAME
#define CONFIG_PTHREAD_SETAFFINITY

#ifdef CONFIG_PTHREAD_SETNAME
 #define _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
 #include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <pthread.h>
#include <limits.h>	/* PTHREAD_STACK_MIN */

#include "ubx.h"
#include "trig_utils.h"
#include "common.h"

#include "types/ptrig_period.h"
#include "types/ptrig_period.h.hexarr"

/* wait 1 second for thread to stop */
#define	THREAD_STOP_TIMEOUT_US	50000
#define	THREAD_STOP_RETRIES	20

char ptrig_meta[] =
	"{ doc='pthread based trigger',"
	"  realtime=true,"
	"}";

ubx_proto_port_t ptrig_ports[] = {
	{ .name = "active_chain", .in_type_name = "int", .doc = "switch the active trigger chain" },
	{ .name = "tstats", .out_type_name = "struct ubx_tstat", .doc = "out port for timing statistics" },
	{ .name = "shutdown", .in_type_name = "int", .doc = "input port for stopping ptrig" },
	{ 0 },
};

ubx_type_t ptrig_types[] = {
	def_struct_type(struct ptrig_period, &ptrig_period_h),
};

def_cfg_getptr_fun(cfg_getptr_ptrig_period, struct ptrig_period);

static void __ptrig_stop(ubx_block_t *b);

ubx_proto_config_t ptrig_config[] = {
	{ .name = "period", .type_name = "struct ptrig_period", .doc = "trigger period in { sec, ns }", },
	{ .name = "stacksize", .type_name = "size_t", .doc = "stacksize as per pthread_attr_setstacksize(3)" },
	{ .name = "sched_priority", .type_name = "int", .doc = "pthread priority" },
	{ .name = "sched_policy", .type_name = "char", .doc = "pthread scheduling policy" },
#ifdef CONFIG_PTHREAD_SETAFFINITY
	{ .name = "affinity", .type_name = "int", .doc = "list of CPUs to set the pthread CPU affinity to" },
#endif
	{ .name = "thread_name", .type_name = "char", .doc = "thread name (for dbg), default is block name" },
	{ .name = "autostop_steps", .type_name = "int64_t", .doc = "if set and > 0, block stops itself after X steps", .max=1 },
	{ .name = "num_chains", .type_name = "int", .max = 1, .doc = "number of trigger chains (def: 1)" },

	{ .name = "tstats_mode", .type_name = "int", .max = 1, .doc = "0: off (def), 1: global only, 2: per block", },
	{ .name = "tstats_profile_path", .type_name = "char", .doc = "directory to write the timing stats file to" },
	{ .name = "tstats_output_rate", .type_name = "double", .max = 1, .doc = "throttle output on tstats port" },
	{ .name = "tstats_skip_first", .type_name = "int", .max=1, .doc = "skip N steps before acquiring stats" },
	{ .name = "loglevel", .type_name = "int" },
	{ 0 },
};


/* used by the thread to reports it's actual state */
enum thread_state {
	THREAD_INACTIVE,
	THREAD_ACTIVE
};

const char* schedpol_tostr(unsigned int schedpol)
{
	switch(schedpol) {
	case SCHED_OTHER: return "SCHED_OTHER";
	case SCHED_FIFO: return "SCHED_FIFO";
	case SCHED_RR: return "SCHED_RR";
	case SCHED_IDLE: return "SCHED_IDLE";
	case SCHED_BATCH: return "SCHED_BATCH";
	default:
		return "unknown";
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

	pthread_mutex_t mutex;
	pthread_cond_t active_cond;

	const struct ptrig_period *period;

	struct ubx_chain *chains;
	int num_chains;
	int actchain;

	int64_t autostop_steps;

	ubx_port_t *p_actchain;
};


/* helper for normalizing struct timespecs */
inline void tsnorm(struct timespec *ts)
{
	if (ts->tv_nsec >= NSEC_PER_SEC) {
		ts->tv_sec += ts->tv_nsec / NSEC_PER_SEC;
		ts->tv_nsec = ts->tv_nsec % NSEC_PER_SEC;
	}
}

/* thread entry */
void *thread_startup(void *arg)
{
	int ret;
	ubx_block_t *b;
	struct ptrig_inf *inf;
	struct ubx_timespec next, period;

	b = (ubx_block_t *) arg;
	inf = (struct ptrig_inf *)b->private_data;

	period.sec = inf->period->sec;
	period.nsec = inf->period->usec * NSEC_PER_USEC;

	while (1) {

		pthread_mutex_lock(&inf->mutex);

		while (inf->state != BLOCK_STATE_ACTIVE) {

			common_output_stats(b, inf->chains, inf->num_chains);
			common_log_stats(b, inf->chains, inf->num_chains);

			ret = common_write_stats(b, inf->chains, inf->num_chains);

			if (ret)
				ubx_err(b, "failed to write tstats to profile_path: %d", ret);

			inf->thread_state = THREAD_INACTIVE;
			pthread_cond_wait(&inf->active_cond, &inf->mutex);
		}
		inf->thread_state = THREAD_ACTIVE;
		pthread_mutex_unlock(&inf->mutex);

		ret = ubx_gettime(&next);

		if (ret) {
			ubx_err(b, "ubx_gettime failed: %s", strerror(errno));
			goto out;
		}

		common_read_actchain(b, inf->p_actchain, inf->num_chains, &inf->actchain);

		if (ubx_chain_trigger(&inf->chains[inf->actchain]) != 0)
			ubx_err(b, "ubx_chain_trigger failed for chain%i", inf->actchain);

		ubx_ts_add(&next, &period, &next);

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

		ret = ubx_nanosleep(TIMER_ABSTIME, &next);

		if (ret) {
			ubx_err(b, "clock_nanosleep failed: %s", strerror(errno));
			goto out;
		}
	}

 out:
	/* block on cond var that signals block is running */
	pthread_exit(NULL);
}

int ptrig_handle_config(ubx_block_t *b)
{
	long len;
	int ret = -EINVALID_CONFIG;
	unsigned int schedpol;
	const int64_t *autostop_steps;
	const char *schedpol_str;
	const size_t *stacksize = NULL;
	const int *prio;
	struct sched_param sched_param; /* prio */
	struct ptrig_inf *inf = (struct ptrig_inf *)b->private_data;

	/* autostop_steps */
	len = cfg_getptr_int64(b, "autostop_steps", &autostop_steps);
	assert(len >= 0);

	inf->autostop_steps = (len > 0) ? *autostop_steps : -1;

	/* period */
	len = cfg_getptr_ptrig_period(b, "period", &inf->period);
	assert(len >= 0);

	if (len == 0) {
		ubx_err(b, "mandatory config 'period' unconfigured");
		goto out;
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

		if (pthread_attr_setstacksize(&inf->attr, *stacksize)) {
			ubx_err(b, "pthread_attr_setstacksize failed: %s",
				strerror(ret));
			goto out;
		}
	}

	/* schedpolicy */
	len = cfg_getptr_char(b, "sched_policy", &schedpol_str);
	assert(len >= 0);

	if (len > 0) {
		if (strncmp(schedpol_str, "SCHED_OTHER", len) == 0) {
			schedpol = SCHED_OTHER;
		} else if (strncmp(schedpol_str, "SCHED_FIFO", len) == 0) {
			schedpol = SCHED_FIFO;
		} else if (strncmp(schedpol_str, "SCHED_RR", len) == 0) {
			schedpol = SCHED_RR;
		} else {
			ubx_err(b, "sched_policy config: illegal value %s",
				schedpol_str);
			goto out;
		}
	} else {
		schedpol = SCHED_OTHER;
	}

	if (pthread_attr_setschedpolicy(&inf->attr, schedpol))
		ubx_err(b, "pthread_attr_setschedpolicy failed");

	/* see PTHREAD_ATTR_SETSCHEDPOLICY(3) */
	ret = pthread_attr_setinheritsched(&inf->attr, PTHREAD_EXPLICIT_SCHED);

	if (ret != 0)
		ubx_err(b, "failed to set PTHREAD_EXPLICIT_SCHED: %s",
			strerror(ret));

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

	ret = pthread_attr_setschedparam(&inf->attr, &sched_param);

	if (ret != 0) {
		ubx_err(b, "failed to set sched_policy.sched_priority to %d: %s",
			sched_param.sched_priority, strerror(ret));
		ret = EINVALID_CONFIG;
		goto out;
	}

	/* log */
	if (stacksize != NULL)
		ubx_info(b, "period: %lus:%luus, policy %s, prio %d, stacksize 0x%zu",
			 inf->period->sec, inf->period->usec,
			 schedpol_tostr(schedpol),
			 sched_param.sched_priority,
			 *stacksize);
	else
		ubx_info(b, "period %lus:%luus, policy %s, prio %d, stacksize default",
			 inf->period->sec, inf->period->usec,
			 schedpol_tostr(schedpol),
			 sched_param.sched_priority);

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

	if (ptrig_handle_config(b) != 0) {
		ret = EINVALID_CONFIG;
		goto out_err;
	}

	/* create thread */
	ret = pthread_create(&inf->tid, &inf->attr, thread_startup, b);

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

#ifdef CONFIG_PTHREAD_SETAFFINITY
	/* cpu affinity */
	const int *aff;
	len = cfg_getptr_int(b, "affinity", &aff);
	assert(len>=0);

	if (len > 0) {
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);

		for (int i=0; i<len; i++) {
			ubx_info(b, "setting affinity to CPU core %i",	aff[i]);
			CPU_SET(aff[i], &cpuset);
		}

		ret = pthread_setaffinity_np(inf->tid, sizeof(cpu_set_t), &cpuset);

		if (ret != 0) {
			ubx_err(b, "pthread_setaffinity_np failed: %s", strerror(ret));
			ret = -1;
			goto out_err;
		}
	} else {
		ubx_debug(b, "setting no thread affinity");
	}
#endif

	/* OK */
	ret = 0;
	goto out;

 out_err:
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

	/* wait some time for thread to shutdown cleanly */
	for (int i=THREAD_STOP_RETRIES; i>=0; i--) {
		if (inf->thread_state == THREAD_INACTIVE)
			return;
		usleep(THREAD_STOP_TIMEOUT_US);
	}
	ubx_warn(b, "timeout waiting for pthread to stop");

	common_unconfig(inf->chains, inf->num_chains);
}

void ptrig_cleanup(ubx_block_t *b)
{
	int ret;

	struct ptrig_inf *inf = (struct ptrig_inf *)b->private_data;

	inf->state = BLOCK_STATE_PREINIT;

	ret = pthread_cancel(inf->tid);

	if (ret != 0)
		ubx_err(b, "pthread_cancel failed: %s", strerror(ret));

	/* join */
	ret = pthread_join(inf->tid, NULL);
	if (ret != 0)
		ubx_err(b, "pthread_join failed: %s", strerror(ret));

	pthread_attr_destroy(&inf->attr);

	/* even though we call ubx_chain_init in start, it is OK to do
	 * this in cleanup only since start calls realloc which will
	 * just resize to the current size */
	common_cleanup(b, &inf->chains);
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
