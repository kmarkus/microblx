/*
 * A fblock that generates random numbers.
 */

#define DEBUG 1

#include <stdio.h>
#include <stdlib.h>

#include "u5c.h"

#include "types/random_config.h"
#include "types/random_config.h.hexarr"

u5c_type_t random_config_type = { 
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
u5c_config_t rnd_config[] = {
	{ .name="random_config", .type_name = "random/struct random_config" },
	{ NULL },
};


u5c_port_t rnd_ports[] = {
	{ .name="seed", .attrs=PORT_DIR_IN, .in_type_name="unsigned int" },
	{ .name="rnd", .attrs=PORT_DIR_OUT, .out_type_name="unsigned int" },
	{ NULL },
};

/* convenience functions to read/write from the ports */
def_read_fun(read_uint, unsigned int)
def_write_fun(write_uint, unsigned int)

static int rnd_init(u5c_block_t *c)
{
	DBG(" ");
	/* alloc all component data here: component, ports, config */
	return 0;
}

static int rnd_start(u5c_block_t *c)
{
	DBG("in");
	uint32_t seed, ret;
	u5c_port_t* seed_port = u5c_port_get(c, "seed");
	ret = read_uint(seed_port, &seed);
	if(ret==PORT_READ_NEWDATA) {
		DBG("starting component. Using seed: %d", seed);
		srandom(seed);
	}
	return 0; /* Ok */
}

static void rnd_step(u5c_block_t *c) { 
	/* cache in instance */
	unsigned int rand_val;

	DBG(" ");

	u5c_port_t* rand_port = u5c_port_get(c, "rnd");

	rand_val = random();
	write_uint(rand_port, &rand_val);
}

static void rnd_cleanup(u5c_block_t *c) { DBG(" "); }

/* put everything together */
u5c_block_t random_comp = {
	.name = "random",
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

static int random_init(u5c_node_info_t* ni)
{
	DBG(" ");	
	u5c_type_register(ni, &random_config_type);
	return u5c_block_register(ni, &random_comp);
}

static void random_cleanup(u5c_node_info_t *ni)
{
	DBG(" ");
	u5c_block_unregister(ni, BLOCK_TYPE_COMPUTATION, "random");
}

module_init(random_init)
module_cleanup(random_cleanup)
