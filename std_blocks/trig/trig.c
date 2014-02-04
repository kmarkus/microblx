/*
 * A pthread based trigger block
 */

#define DEBUG 1
#define CONFIG_PTHREAD_SETNAME

#ifdef CONFIG_PTHREAD_SETNAME
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>	/* PTHREAD_STACK_MIN */

#include "ubx.h"

#include "types/trig_config.h"
#include "types/trig_config.h.hexarr"
#include "types/trig_tstat.h"
#include "types/trig_tstat.h.hexarr"

/* trig metadata */
char trig_meta[] =
	"{ doc='simple, activity-less trigger',"
	"  real-time=true,"
	"}";

ubx_port_t trig_ports[] = {
	{ .name="tstats", .out_type_name="struct trig_tstat", .doc="timing statistics (if enabled)"},
	{ NULL },
};

/* types defined by trig block */
ubx_type_t trig_types[] = {
	def_struct_type(struct trig_config, &trig_config_h),
	def_struct_type(struct trig_tstat, &trig_tstat_h),
	{ NULL },
};

/* configuration */
ubx_config_t trig_config[] = {
	{ .name="trig_blocks", .type_name = "struct trig_config", .doc="describes which blocks to trigger" },
	{ .name="time_stats_enabled", .type_name = "int", .doc="set to 1 to enable min/max/avg trigger execution measurements" },
	{ NULL },
};

/* instance state */
struct trig_inf {
	struct trig_config *trig_list;
	unsigned int trig_list_len;

	struct trig_tstat tstats;

	ubx_port_t *p_tstats;
};

def_write_fun(write_tstat, struct trig_tstat)

inline void tsnorm(struct timespec *ts)
{
	if(ts->tv_nsec >= NSEC_PER_SEC) {
		ts->tv_sec+=ts->tv_nsec / NSEC_PER_SEC;
		ts->tv_nsec=ts->tv_nsec % NSEC_PER_SEC;
	}
}

/* trigger the configured blocks */
int do_trigger(struct trig_inf *inf)
{
	int i, steps, ret=-1;
	struct ubx_timespec ts_start, ts_end, ts_dur;

	ubx_clock_mono_gettime(&ts_start);

	for(i=0; i<inf->trig_list_len; i++) {
		for(steps=0; steps<inf->trig_list[i].num_steps; steps++) {
			if(ubx_cblock_step(inf->trig_list[i].b)!=0)
				goto out;
		}
	}

	/* compute tstats */
	ubx_clock_mono_gettime(&ts_end);
	ubx_ts_sub(&ts_end, &ts_start, &ts_dur);

	if(ubx_ts_cmp(&ts_dur, &inf->tstats.min) == -1)
		inf->tstats.min=ts_dur;

	if(ubx_ts_cmp(&ts_dur, &inf->tstats.max) == 1)
		inf->tstats.max=ts_dur;

	ubx_ts_add(&inf->tstats.total, &ts_dur, &inf->tstats.total);
	inf->tstats.cnt++;
	write_tstat(inf->p_tstats, &inf->tstats);

	ret=0;
 out:
	return ret;
}

/* step */
void trig_step(ubx_block_t *b)
{
	DBG(" ");
	struct trig_inf *inf;
	inf = (struct trig_inf*) b->private_data;

	if(do_trigger(inf) != 0)
		ERR("do_trigger failed");
}


/* init */
int trig_init(ubx_block_t *b)
{
	int ret = EOUTOFMEM;
	struct trig_inf* inf;

	if((b->private_data=calloc(1, sizeof(struct trig_inf)))==NULL) {
		ERR("failed to alloc");
		goto out;
	}

	inf=(struct trig_inf*) b->private_data;

	inf->tstats.min.sec=999999999;
	inf->tstats.min.nsec=999999999;

	inf->tstats.max.sec=0;
	inf->tstats.max.nsec=0;

	inf->tstats.total.sec=0;
	inf->tstats.total.nsec=0;

	/* OK */
	ret=0;
	goto out;

 out:
	return ret;
}

int trig_start(ubx_block_t *b)
{
	DBG(" ");

	struct trig_inf *inf;
	ubx_data_t* trig_list_data;

	inf = (struct trig_inf*) b->private_data;

	trig_list_data = ubx_config_get_data(b, "trig_blocks");

	/* make a copy? */
	inf->trig_list = trig_list_data->data;
	inf->trig_list_len = trig_list_data->len;

	assert(inf->p_tstats = ubx_port_get(b, "tstats"));

	return 0;
}

void trig_cleanup(ubx_block_t *b)
{
	free(b->private_data);
}


/* put everything together */
ubx_block_t trig_comp = {
	.name = "std_triggers/trig",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = trig_meta,
	.configs = trig_config,
	.ports = trig_ports,

	.init = trig_init,
	.start = trig_start,
	.cleanup = trig_cleanup,
	.step = trig_step
};

int trig_mod_init(ubx_node_info_t* ni)
{
	int ret;
	ubx_type_t *tptr;

	for(tptr=trig_types; tptr->name!=NULL; tptr++) {
		if((ret=ubx_type_register(ni, tptr))!=0) {
			ERR("failed to register type %s", tptr->name);
			goto out;
		}
	}
	ret=ubx_block_register(ni, &trig_comp);
 out:
	return ret;
}

void trig_mod_cleanup(ubx_node_info_t *ni)
{
	ubx_type_t *tptr;

	for(tptr=trig_types; tptr->name!=NULL; tptr++)
		ubx_type_unregister(ni, tptr->name);

	ubx_block_unregister(ni, "std_triggers/trig");
}

UBX_MODULE_INIT(trig_mod_init)
UBX_MODULE_CLEANUP(trig_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
