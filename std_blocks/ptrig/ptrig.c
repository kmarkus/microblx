/*
 * A pthread based trigger block
 */

/* #define DEBUG 1 */
#define CONFIG_PTHREAD_SETNAME

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

#include "types/ptrig_config.h"
#include "types/ptrig_config.h.hexarr"

#include "types/ptrig_period.h"
#include "types/ptrig_period.h.hexarr"

#include "../../std_types/stattypes/types/ubx_tstat.h"
#include "../../std_types/stattypes/types/ubx_tstat.h.hexarr"

#include "ubx_tstat.h"

/* ptrig metadata */
char ptrig_meta[] =
	"{ doc='pthread based trigger',"
	"  realtime=true,"
	"}";

ubx_port_t ptrig_ports[] = {
	{ .name="tstats",
	  .out_type_name="struct ubx_tstat",
	  .doc="out port for totals and per block timing statistics"
	},
	{ .name="shutdown",
	  .in_type_name="int",
	  .doc="in port for stopping ptrig"
	},
	{ NULL },
};

/* types defined by ptrig block */
ubx_type_t ptrig_types[] = {
	def_struct_type(struct ptrig_config, &ptrig_config_h),
	def_struct_type(struct ptrig_period, &ptrig_period_h),
	{ NULL },
};

/* configuration */
ubx_config_t ptrig_config[] = {
	{ .name="period", .type_name = "struct ptrig_period",
	  .doc="trigger period in { sec, ns }",
	},
	{ .name="stacksize", .type_name = "size_t",
	  .doc="stacksize as per pthread_attr_setstacksize(3)"
	},
	{ .name="sched_priority", .type_name = "int" },
	{ .name="sched_policy", .type_name = "char" },
	{ .name="thread_name", .type_name = "char" },
	{ .name="trig_blocks", .type_name = "struct ptrig_config",
	  .doc="trigger conf: which block and how to trigger"
	},
	{ .name="profile_path", .type_name = "char" },
	{ .name="tstats_enabled", .type_name = "int",
	  .doc="enable timing statistics over all blocks",
	},
	{ .name="tstats_output_rate",
	  .type_name = "unsigned int",
	  .doc="output tstats only on every tstats_output_rate'th trigger (0 to disable)"
	},
	{ .name="tstats_print_on_stop",
	  .type_name = "int",
	  .doc="print tstats in stop()",
	},
	{ NULL },
};


const char* tstat_global_id = "##total##";

/* instance state */
struct ptrig_inf {
	pthread_t tid;
	pthread_attr_t attr;
	uint32_t state;

	pthread_mutex_t mutex;
	pthread_cond_t active_cond;

	const struct ptrig_config *trig_list;
	unsigned int trig_list_len;

	struct ptrig_period* period;

	/* timing statistics */
	const char *profile_path;
	int tstats_enabled;
	int tstats_print_on_stop;

	struct ubx_tstat global_tstats;
	struct ubx_tstat *blk_tstats;

	ubx_port_t *p_tstats;
};

def_write_fun(write_tstat, struct ubx_tstat)

/**
 * print tstats
 */
void ptrig_tstats_print(struct ptrig_inf *inf)
{
	unsigned int i;

	if (!inf->tstats_print_on_stop)
		goto out;

	if (inf->tstats_enabled) {
		for (i = 0; i < inf->trig_list_len; i++)
			if (inf->trig_list[i].measure)
				tstat_print(inf->profile_path, &inf->blk_tstats[i]);
	}

out:
	return;
}

/* trigger the configured blocks */
static int trigger_steps(struct ptrig_inf *inf)
{
	int ret=-1;
	unsigned int i, steps;

	struct ubx_timespec ts_start, ts_end, blk_ts_start, blk_ts_end;

	if(inf->tstats_enabled) {
		ubx_gettime(&ts_start);
	}

	for(i=0; i<inf->trig_list_len; i++) {

		if(inf->trig_list[i].measure)
			ubx_gettime(&blk_ts_start);

		for(steps=0; steps<inf->trig_list[i].num_steps; steps++) {

			if(ubx_cblock_step(inf->trig_list[i].b)!=0)
				goto out;

			/* per block statistics */
			if(inf->trig_list[i].measure) {
				ubx_gettime(&blk_ts_end);
				tstat_update(&inf->blk_tstats[i],
					     &blk_ts_start,
					     &blk_ts_end);
			}

		}
	}

	/* compute global tstats */
	if(inf->tstats_enabled) {
		ubx_gettime(&ts_end);
		tstat_update(&inf->global_tstats, &ts_start, &ts_end);

		/* output global tstats - TODO throttling */
		write_tstat(inf->p_tstats, &inf->global_tstats);
	}

	/* output stats - TODO throttling */
	for(i=0; i<inf->trig_list_len; i++) {
		if(inf->trig_list[i].measure)
			write_tstat(inf->p_tstats, &inf->blk_tstats[i]);
	}

	ret=0;
 out:
	return ret;
}

/* helper for normalizing struct timespecs */
static inline void tsnorm(struct timespec *ts)
{
	if(ts->tv_nsec >= NSEC_PER_SEC) {
		ts->tv_sec+=ts->tv_nsec / NSEC_PER_SEC;
		ts->tv_nsec=ts->tv_nsec % NSEC_PER_SEC;
	}
}

/* thread entry */
static void* thread_startup(void *arg)
{
	int ret = -1;
	ubx_block_t *b;
	struct ptrig_inf *inf;
	struct timespec ts;

	b = (ubx_block_t*) arg;
	inf = (struct ptrig_inf*) b->private_data;

	while(1) {
		pthread_mutex_lock(&inf->mutex);

		while(inf->state != BLOCK_STATE_ACTIVE) {
			ptrig_tstats_print(inf);
			pthread_cond_wait(&inf->active_cond, &inf->mutex);
		}
		pthread_mutex_unlock(&inf->mutex);

		if((ret=clock_gettime(CLOCK_MONOTONIC, &ts))) {
			ERR2(ret, "clock_gettime failed");
			goto out;
		}

		trigger_steps(inf);

		ts.tv_sec += inf->period->sec;
		ts.tv_nsec += inf->period->usec*NSEC_PER_USEC;
		tsnorm(&ts);

		if((ret=clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL))) {
			ERR2(ret, "clock_nanosleep failed");
			goto out;
		}
	}

 out:
	/* block on cond var that signals block is running */
	pthread_exit(NULL);
}

int ptrig_handle_config(ubx_block_t *b)
{
	int ret = -1;
	unsigned int schedpol;
	long int tmplen;
	const char *schedpol_str;
	size_t *stacksize;
	const int *prio;
	struct sched_param sched_param; /* prio */
	struct ptrig_inf *inf=(struct ptrig_inf*) b->private_data;

	/* period */
	if((tmplen = ubx_config_get_data_ptr(b, "period", (void**) &inf->period)) <= 0) {
		ERR("%s: config 'period' unconfigured", b->name);
		goto out;
	}

	/* stacksize */
	if((tmplen = ubx_config_get_data_ptr(b, "stacksize", (void**) &stacksize)) < 0)
		goto out;

	if(tmplen > 0) {
		if(*stacksize<PTHREAD_STACK_MIN) {
			ERR("%s: stacksize (%zd) less than PTHREAD_STACK_MIN (%d)",
			    b->name, *stacksize, PTHREAD_STACK_MIN);
			goto out;
		}

		if(pthread_attr_setstacksize(&inf->attr, *stacksize)) {
			ERR2(errno, "pthread_attr_setstacksize failed");
			goto out;
		}
	}

	/* schedpolicy */
	if((tmplen = cfg_getptr_char(b, "sched_policy", &schedpol_str)) < 0)
		goto out;

	if(tmplen > 0) {
		if (strncmp(schedpol_str, "SCHED_OTHER", tmplen) == 0) {
			schedpol = SCHED_OTHER;
		} else if (strncmp(schedpol_str, "SCHED_FIFO", tmplen) == 0) {
			schedpol = SCHED_FIFO;
		} else if (strncmp(schedpol_str, "SCHED_RR", tmplen) == 0) {
			schedpol = SCHED_RR;
		} else {
			ERR("%s: sched_policy config: illegal value %s",
			    b->name, schedpol_str);
			goto out;
		}
	} else {
		schedpol = SCHED_OTHER;
	}

	if(pthread_attr_setschedpolicy(&inf->attr, schedpol)) {
		ERR("pthread_attr_setschedpolicy failed");
	}

	/* see PTHREAD_ATTR_SETSCHEDPOLICY(3) */
	ret = pthread_attr_setinheritsched(&inf->attr, PTHREAD_EXPLICIT_SCHED);

	if(ret!=0)
		ERR2(ret, "failed to set PTHREAD_EXPLICIT_SCHED.");

	/* priority */
	if((tmplen = cfg_getptr_int(b, "sched_priority", &prio)) < 0)
		goto out;

	sched_param.sched_priority = (tmplen > 0) ? *prio : 0;

	if(((schedpol==SCHED_FIFO ||
	     schedpol==SCHED_RR) && sched_param.sched_priority == 0) ||
	   (schedpol==SCHED_OTHER && sched_param.sched_priority > 0)) {
		ERR("%s sched_priority is %d with %s policy",
		    b->name, sched_param.sched_priority, schedpol_str);
	}

	if(pthread_attr_setschedparam(&inf->attr, &sched_param))
		ERR("failed to set sched_policy.sched_priority to %d",
		    sched_param.sched_priority);

	DBG("%s config: period=%lus:%luus, policy=%s, \
	     prio=%d, stacksize=%lu (0=default size)",
	    b->name, inf->period->sec, inf->period->usec,
	    schedpol_str, sched_param.sched_priority,
	    (unsigned long) stacksize);

	ret = 0;
out:
	return ret;
}

/* init */
static int ptrig_init(ubx_block_t *b)
{
	long int len;
	int ret = EOUTOFMEM;
	const char* threadname;
	struct ptrig_inf* inf;

	if((b->private_data=calloc(1, sizeof(struct ptrig_inf)))==NULL) {
		ERR("failed to alloc");
		goto out;
	}

	inf=(struct ptrig_inf*) b->private_data;

	inf->state=BLOCK_STATE_INACTIVE;

	pthread_cond_init(&inf->active_cond, NULL);
	pthread_mutex_init(&inf->mutex, NULL);
	pthread_attr_init(&inf->attr);
	pthread_attr_setdetachstate(&inf->attr, PTHREAD_CREATE_JOINABLE);

	if(ptrig_handle_config(b) != 0) {
		ret = EINVALID_CONFIG;
		goto out_err;
	}

	if((ret=pthread_create(&inf->tid, &inf->attr, thread_startup, b))!=0) {
		ERR2(ret, "pthread_create failed");
		goto out_err;
	}

#ifdef CONFIG_PTHREAD_SETNAME
	if((len = cfg_getptr_char(b, "thread_name", &threadname)) < 0)
		goto out_err;

	threadname = (len>0) ? threadname : b->name;

	if(pthread_setname_np(inf->tid, threadname))
		ERR("failed to set thread_name to %s", threadname);
#endif

	/* OK */
	ret=0;
	goto out;

 out_err:
	free(b->private_data);
 out:
	return ret;
}

static int ptrig_start(ubx_block_t *b)
{
	DBG(" ");
	int ret = -1;
	const int *val;
	long int len;
	struct ptrig_inf *inf;
	FILE *fp;

	inf = (struct ptrig_inf*) b->private_data;

	len = ubx_config_get_data_ptr(b, "trig_blocks", (void**) &inf->trig_list);

	if(len < 0)
		goto out;

	inf->trig_list_len = len;

	/* preparing timing statistics */
	tstat_init(&inf->global_tstats, tstat_global_id);

	/* this is freed in cleanup hook */
	inf->blk_tstats = realloc(
		inf->blk_tstats,
		inf->trig_list_len * sizeof(struct ubx_tstat));

	if(!inf->blk_tstats) {
		ERR("failed to alloc blk_stats");
		goto out;
	}

	for(unsigned int i=0; i<inf->trig_list_len; i++) {
		tstat_init(&inf->blk_tstats[i],
			   inf->trig_list[i].b->name);
	}

	if ((len = cfg_getptr_char(b, "profile_path", &inf->profile_path)) < 0) {
		ERR("unable to retrieve profile_path parameter");
		goto out;
	}
	/* truncate the file if it exists */
	if (len > 0) {
		fp = fopen(inf->profile_path, "w");
		if (fp)
			fclose(fp);
	}

	if((len = cfg_getptr_int(b, "tstats_print_on_stop", &val)) < 0)
		goto out;

	inf->tstats_print_on_stop = (len > 0) ? *val : 0;

	if((len = cfg_getptr_int(b, "tstats_enabled", &val)) < 0)
		goto out;

	inf->tstats_enabled = (len > 0) ? *val : 0;

	if(inf->tstats_enabled) {
		DBG("tstats enabled");
	}

	pthread_mutex_lock(&inf->mutex);
	inf->state=BLOCK_STATE_ACTIVE;
	pthread_cond_signal(&inf->active_cond);
	pthread_mutex_unlock(&inf->mutex);

	inf->p_tstats = ubx_port_get(b, "tstats");
	ret = 0;

out:
	return ret;
}

static void ptrig_stop(ubx_block_t *b)
{
	DBG(" ");
	struct ptrig_inf *inf;

	inf = (struct ptrig_inf*) b->private_data;

	pthread_mutex_lock(&inf->mutex);
	inf->state=BLOCK_STATE_INACTIVE;
	pthread_mutex_unlock(&inf->mutex);
}

static void ptrig_cleanup(ubx_block_t *b)
{
	DBG(" ");
	int ret;
	struct ptrig_inf* inf;
	inf=(struct ptrig_inf*) b->private_data;

	inf->state=BLOCK_STATE_PREINIT;

	if((ret=pthread_cancel(inf->tid))!=0)
		ERR2(ret, "pthread_cancel failed");

	/* join */
	if((ret=pthread_join(inf->tid, NULL))!=0)
		ERR2(ret, "pthread_join failed");

	pthread_attr_destroy(&inf->attr);

	free(inf->blk_tstats);
	free(b->private_data);
}

/* put everything together */
ubx_block_t ptrig_comp = {
	.name = "std_triggers/ptrig",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = ptrig_meta,

	.configs = ptrig_config,
	.ports = ptrig_ports,

	.init = ptrig_init,
	.start = ptrig_start,
	.stop = ptrig_stop,
	.cleanup = ptrig_cleanup
};

static int ptrig_mod_init(ubx_node_info_t* ni)
{
	int ret;
	ubx_type_t *tptr;

	for(tptr=ptrig_types; tptr->name!=NULL; tptr++) {
		if((ret=ubx_type_register(ni, tptr))!=0) {
			ERR("failed to register type %s", tptr->name);
			goto out;
		}
	}
	ret=ubx_block_register(ni, &ptrig_comp);
	if(ret != 0) {
		ERR("failed to register block");
	}
 out:
	return ret;
}

static void ptrig_mod_cleanup(ubx_node_info_t *ni)
{
	ubx_type_t *tptr;

	for(tptr=ptrig_types; tptr->name!=NULL; tptr++)
		ubx_type_unregister(ni, tptr->name);

	ubx_block_unregister(ni, "std_triggers/ptrig");
}

UBX_MODULE_INIT(ptrig_mod_init)
UBX_MODULE_CLEANUP(ptrig_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
