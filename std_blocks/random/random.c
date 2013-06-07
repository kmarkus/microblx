/*
 * A fblock that generates random numbers.
 */

#define DEBUG 1

#include <stdio.h>
#include <stdlib.h>

#include "u5c.h"

#include "random_config.h"

#define def_basic_ctype(typename) { .name=#typename, .type_class=TYPE_CLASS_STRUCT, .size=sizeof(typename) }

/* declare types */
const u5c_type_t basic_types[] = {
	def_basic_ctype(char),	     def_basic_ctype(unsigned char),	   def_basic_ctype(signed char),
	def_basic_ctype(short),      def_basic_ctype(unsigned short),	   def_basic_ctype(signed short),
	def_basic_ctype(int), 	     def_basic_ctype(unsigned int),	   def_basic_ctype(signed int),
	def_basic_ctype(long),       def_basic_ctype(unsigned long),	   def_basic_ctype(signed long),
	def_basic_ctype(long long),  def_basic_ctype(unsigned long long),  def_basic_ctype(signed long long),
	def_basic_ctype(float),      def_basic_ctype(double),
};

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
	{ .name="seed", .attrs=PORT_DIR_IN, .in_type_name="long int" },
	{ .name="rnd", .attrs=PORT_DIR_OUT, .out_type_name="long int" },
	{ NULL },
};

def_read_fun(read_uint, unsigned int)
def_write_fun(write_longint, long int)

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
