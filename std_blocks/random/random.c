/*
 * A fblock that generates random numbers.
 *
 * This is to be a well (over) documented block to serve as a good
 * example.
 */

/* #define DEBUG 1 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "ubx.h"


/* declare and initialize a microblx type. This will be registered /
 * deregistered in the module init / cleanup at the end of this
 * file.
 *
 * Include regular header file and it's char array representation
 * (used for luajit reflection, logging, etc.)
 */
#include "types/random_config.h"
#include "types/random_config.h.hexarr"

/* declare the type and give the char array type representation as the type private_data */
ubx_type_t random_config_type = def_struct_type(struct random_config, &random_config_h);

/* function block meta-data
 * used by higher level functions.
 */
char rnd_meta[] =
	"{ doc='A random number generator function block',"
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
	{ .name="min_max_config", .type_name = "struct random_config" },
	{ NULL },
};

/* Ports
 */
ubx_port_t rnd_ports[] = {
	{ .name="seed", .in_type_name="unsigned int" },
	{ .name="rnd", .out_type_name="unsigned int" },
	{ NULL },
};

/* block local info
 *
 * This struct holds the information needed by the hook functions
 * below.
 */
struct random_info {
	int min;
	int max;
};

/* convenience functions to read/write from the ports these fill a
 * ubx_data_t, and call port->[read|write](&data). These introduce
 * some type safety.
 */
def_read_fun(read_uint, unsigned int)
def_write_fun(write_uint, unsigned int)

/**
 * rnd_init - block init function.
 *
 * for RT blocks: any memory should be allocated here.
 *
 * @param b
 *
 * @return Ok if 0,
 */
static int rnd_init(ubx_block_t *b)
{
	int ret=0;

	DBG(" ");
	if ((b->private_data = calloc(1, sizeof(struct random_info)))==NULL) {
		ERR("Failed to alloc memory");
		ret=EOUTOFMEM;
		goto out;
	}
 out:
	return ret;
}

/**
 * rnd_cleanup - cleanup block.
 *
 * for RT blocks: free all memory here
 *
 * @param b
 */
static void rnd_cleanup(ubx_block_t *b)
{
	DBG(" ");
	free(b->private_data);
}

/**
 * rnd_start - start the random block.
 *
 * @param b
 *
 * @return 0 if Ok, if non-zero block will not be started.
 */
static int rnd_start(ubx_block_t *b)
{
	DBG("in");
	uint32_t seed, ret;
	unsigned int clen;
	struct random_config* rndconf;
	struct random_info* inf;

	inf=(struct random_info*) b->private_data;

	/* get and store min_max_config */
	rndconf = (struct random_config*) ubx_config_get_data_ptr(b, "min_max_config", &clen);
	inf->min = rndconf->min;
	inf->max = (rndconf->max == 0) ? INT_MAX : rndconf->max;

	/* seed is allowed to change at runtime, check if new one available */
	ubx_port_t* seed_port = ubx_port_get(b, "seed");
	ret = read_uint(seed_port, &seed);

	if(ret>0) {
		DBG("starting component. Using seed: %d, min: %d, max: %d", seed, inf->min, inf->max);
		srandom(seed);
	} else {
		DBG("starting component. Using min: %d, max: %d", inf->min, inf->max);
	}
	return 0; /* Ok */
}

/**
 * rnd_step - this function implements the main functionality of the
 * block. Ports are read and written here.
 *
 * @param b
 */
static void rnd_step(ubx_block_t *b) {
	unsigned int rand_val;
	struct random_info* inf;

	inf=(struct random_info*) b->private_data;

	ubx_port_t* rand_port = ubx_port_get(b, "rnd");
	rand_val = random();
	rand_val = (rand_val > inf->max) ? (rand_val%inf->max) : rand_val;
	rand_val = (rand_val < inf->min) ? ((inf->min + rand_val)%inf->max) : rand_val;

	write_uint(rand_port, &rand_val);
}


/* put everything together
 *
 */
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

/**
 * rnd_module_init - initialize module
 *
 * here types and blocks are registered.
 *
 * @param ni
 *
 * @return 0 if OK, non-zero otherwise (this will prevent the loading of the module).
 */
static int rnd_module_init(ubx_node_info_t* ni)
{
	ubx_type_register(ni, &random_config_type);
	return ubx_block_register(ni, &random_comp);
}

/**
 * rnd_module_cleanup - de
 *
 * unregister blocks.
 *
 * @param ni
 */
static void rnd_module_cleanup(ubx_node_info_t *ni)
{
	ubx_type_unregister(ni, "struct random_config");
	ubx_block_unregister(ni, "random/random");
}

/* declare the module init and cleanup function */
UBX_MODULE_INIT(rnd_module_init)
UBX_MODULE_CLEANUP(rnd_module_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
