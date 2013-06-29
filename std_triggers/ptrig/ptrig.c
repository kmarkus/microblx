/*
 * A pthread based trigger block
 */

#define DEBUG 1

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "u5c.h"

char ptrig_meta[] =
	"{ doc='pthread based trigger',"
	"  license='MIT',"
	"  real-time=false,"
	"}";

/* configuration */
u5c_config_t ptrig_config[] = {
	{ .name="stacksize", .type_name = "size_t" },
	{ .name="sched_priority", .type_name = "int" },
	{ .name="sched_policy", .type_name = "char", .value = { .len=12 } },
	{ .name="trig_blocks", .type_name = "char" },
	{ NULL },
};

struct ptrig_inf {
	pthread_t tid;
	pthread_attr_t attr;
	uint32_t state;
	pthread_mutex_t mutex;
	pthread_cond_t active_cond;
	u5c_block_t** trig_blocks;
};

/* thread entry */
static void* thread_startup(void *arg)
{
	u5c_block_t *b;
	struct ptrig_inf *inf;

	b = (u5c_block_t*) arg;
	inf = (struct ptrig_inf*) b->private_data;

	while(1) {
		pthread_mutex_lock(&inf->mutex);

		while(inf->state != BLOCK_STATE_ACTIVE) {
			pthread_cond_wait(&inf->active_cond, &inf->mutex);
		}
		pthread_mutex_unlock(&inf->mutex);

		DBG("triggering steps");

		/* trigger step's */
	}

	/* block on cond var that signals block is running */
	pthread_exit(NULL);
}

/* init */
static int ptrig_init(u5c_block_t *b)
{
	int ret = EOUTOFMEM;
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
	
	/* TODO: read config attrs */

	if((ret=pthread_create(&inf->tid, &inf->attr, thread_startup, b))!=0) {
		ERR2(ret, "pthread_create failed");
		goto out_err;
	}

	/* OK */
	ret=0;
	goto out;

 out_err:
	free(b->private_data);
 out:
	return ret;
}

static int ptrig_start(u5c_block_t *b)
{
	struct ptrig_inf *inf;
	inf = (struct ptrig_inf*) b->private_data;
	
	pthread_mutex_lock(&inf->mutex);
	inf->state=BLOCK_STATE_ACTIVE;
	pthread_cond_signal(&inf->active_cond);
	pthread_mutex_unlock(&inf->mutex);
	return 0;
}

static void ptrig_stop(u5c_block_t *b)
{
	struct ptrig_inf *inf;
	inf = (struct ptrig_inf*) b->private_data;
	
	pthread_mutex_lock(&inf->mutex);
	inf->state=BLOCK_STATE_INACTIVE;
	pthread_mutex_unlock(&inf->mutex);
}

static void ptrig_cleanup(u5c_block_t *b)
{
	int ret;
	struct ptrig_inf* inf;
	inf=(struct ptrig_inf*) b->private_data;

	inf->state=BLOCK_STATE_PREINIT;

	if((ret=pthread_cancel(inf->tid))!=0)
		ERR2(ret, "pthread_cancel failed");
	
	/* join */
	if((ret=pthread_join(inf->tid, NULL))!=0)
		ERR2(ret, "pthread_join failed");

	free(b->private_data);
}

/* put everything together */
u5c_block_t ptrig_comp = {
	.name = "std_triggers/ptrig",
	.type = BLOCK_TYPE_TRIGGER,
	.meta_data = ptrig_meta,
	.configs = ptrig_config,
	.init = ptrig_init,
	.start = ptrig_start,
	.stop = ptrig_stop,
	.cleanup = ptrig_cleanup
};

static int pthread_mod_init(u5c_node_info_t* ni)
{
	DBG(" ");	
	return u5c_block_register(ni, &ptrig_comp);
}

static void pthread_mod_cleanup(u5c_node_info_t *ni)
{
	DBG(" ");
	u5c_block_unregister(ni, "std_triggers/ptrig");
}

module_init(pthread_mod_init)
module_cleanup(pthread_mod_cleanup)
