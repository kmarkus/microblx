/*
 * A fblock that generates random numbers.
 */

#define DEBUG 1

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "ubx.h"

#include "types/random_config.h"
#include "types/random_config.h.hexarr"

ubx_type_t random_config_type = {
	.name="random/struct random_config",
	.type_class=TYPE_CLASS_STRUCT,
	.size=sizeof(struct random_config),
	.private_data = &random_config_h,
};

/* #define def_struct_ctype(modulename, typename) */
/* { \ */
/* 	.name=#module "/" #typename, \ */
/* 	.type_class=TYPE_CLASS_STRUCT, \ */
/*      .size=sizeof(typename), \ */
/* 	.private_data=&random_config_h, \ */
/* }; \ */


/* function block meta-data
 * used by higher level functions.
 */
char rnd_meta[] =
	"{ doc='A random number generator function block',"
	"  license='LGPL',"
	"  real-time=true,"
	"}";

/* configuration
 * upon cloning the following happens:
 *   - value.type is resolved
 *   - value.data will point to a buffer of size value.len*value.type->size
 *
 * if an array is required, then .value = { .len=<LENGTH> } can be used.
 */
ubx_config_t rnd_config[] = {
	{ .name="min_max_config", .type_name = "random/struct random_config" },
	{ NULL },
};


ubx_port_t rnd_ports[] = {
	{ .name="seed", .attrs=PORT_DIR_IN, .in_type_name="unsigned int" },
	{ .name="rnd", .attrs=PORT_DIR_OUT, .out_type_name="unsigned int" },
	{ NULL },
};

struct random_info {
	int min;
	int max;
};

/* convenience functions to read/write from the ports */
def_read_fun(read_uint, unsigned int)
def_write_fun(write_uint, unsigned int)

static int rnd_init(ubx_block_t *c)
{
	int ret=0;

	DBG(" ");
	if ((c->private_data = calloc(1, sizeof(struct random_info)))==NULL) {
		ERR("Failed to alloc memory");
		ret=EOUTOFMEM;
		goto out;
	}


 out:
	return ret;
}


static void rnd_cleanup(ubx_block_t *c)
{
	DBG(" ");
	free(c->private_data);
}

static int rnd_start(ubx_block_t *c)
{
	DBG("in");
	uint32_t seed, ret;
	unsigned int clen;
	struct random_config* rndconf;
	struct random_info* inf;

	inf=(struct random_info*) c->private_data;

	/* get and store min_max_config */
	rndconf = (struct random_config*) ubx_config_get_data_ptr(c, "min_max_config", &clen);
	inf->min = rndconf->min;
	inf->max = (rndconf->max == 0) ? INT_MAX : rndconf->max;

	/* seed is allowed to change at runtime, check if new one available */
	ubx_port_t* seed_port = ubx_port_get(c, "seed");
	ret = read_uint(seed_port, &seed);
	if(ret==PORT_READ_NEWDATA) {
		DBG("starting component. Using seed: %d, min: %d, max: %d", seed, inf->min, inf->max);
		srandom(seed);
	} else {
		DBG("starting component. Using min: %d, max: %d", inf->min, inf->max);
	}
	return 0; /* Ok */
}

static void rnd_step(ubx_block_t *c) {
	unsigned int rand_val;
	struct random_info* inf;

	inf=(struct random_info*) c->private_data;

	ubx_port_t* rand_port = ubx_port_get(c, "rnd");
	rand_val = random();
	rand_val = (rand_val > inf->max) ? (rand_val%inf->max) : rand_val;
	rand_val = (rand_val < inf->min) ? ((inf->min + rand_val)%inf->max) : rand_val;

	write_uint(rand_port, &rand_val);
}


/* put everything together */
ubx_block_t random_comp = {
	.name = "random/random",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = rnd_meta,
	.configs = rnd_config,
	.ports = rnd_ports,

	/* ops */
	.init = rnd_init,
	.start = rnd_start,
	.step = rnd_step,
	.cleanup = rnd_cleanup,
};

static int random_init(ubx_node_info_t* ni)
{
	DBG(" ");
	ubx_type_register(ni, &random_config_type);
	return ubx_block_register(ni, &random_comp);
}

static void random_cleanup(ubx_node_info_t *ni)
{
	DBG(" ");
	ubx_block_unregister(ni, "random/random");
}

module_init(random_init)
module_cleanup(random_cleanup)
