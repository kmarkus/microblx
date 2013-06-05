/*
 * A fblock that generates random numbers.
 */

#define DEBUG 1

#include <stdio.h>
#include <stdlib.h>

#include "u5c.h"

#include "random_config.h"

/* static u5c_port pout_random = { .name="random", .attrs=PORT_DIR_OUT, .data_type="long int" }; */

char rnd_meta[] =
	"{ doc='A random number generator function block',"
	"  license='LGPL',"
	"  real-time=true,"
	"}";

/* declare configuration */
struct rand_config config;

const u5c_config_t rnd_config[] = {
	{ .type_name = "random/struct rand_config", .data = &config },
	{ NULL },
};

u5c_port_t rnd_ports[] = {
	{ .name="seed", .attrs=PORT_DIR_IN, .in_type_name="unsigned int" },
	{ .name="rnd", .attrs=PORT_DIR_OUT, .out_type_name="unsigned int" },
	{ NULL },
};

gen_read(read_uint, unsigned int)
gen_write(write_longint, long int)

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
	if(ret==PORT_READ_NEWDATA)
		srandom(seed);
	DBG("starting component. Using seed: %d", seed);
	return 0; /* Ok */
}

static void rnd_step(u5c_block_t *c) { 
	/* cache in instance */
	long int rand_val;

	DBG(" ");

	u5c_port_t* rand_port = u5c_port_get(c, "rnd");

	rand_val = random();
	write_longint(rand_port, &rand_val);
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
	return u5c_block_register(ni, &random_comp);
}

static void random_cleanup(u5c_node_info_t *ni)
{
	DBG(" ");
	u5c_block_unregister(ni, BLOCK_TYPE_COMPUTATION, "random");
}

module_init(random_init)
module_cleanup(random_cleanup)
