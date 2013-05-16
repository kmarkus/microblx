/*
 * A fblock that generates random numbers.
 */



#define DEBUG 1
#include <stdio.h>
#include "u5c.h"

#include "struct_rand_config.h"

/* static u5c_port pout_random = { .name="random", .attrs=PORT_DIR_OUT, .data_type="long int" }; */

u5c_port ports[] = {
	{ .name="seed", .attrs=PORT_DIR_IN, .data_type="unsigned int" },
	{ .name="output", .attrs=PORT_DIR_OUT, .data_type="unsigned int" },
	{ NULL },
};

struct rand_config config = { .min = 10, .max=1000 };

static int init(u5c_component *c)
{
	DBG(" ");
	/* this is dynamic port creation:
	   c->ports = malloc(3*sizeof(u5c_port*));
	   c->ports[0] = alloc_port("random", PORT_DIR_OUT, "int");
	   c->ports[1] = alloc_port("seed", PORT_DIR_IN, "long int");
	   c->ports[2] = NULL;
	*/

	/* alloc all component data here: component, ports, config */
	return 0;
}

static void exec(u5c_component *c) { DBG(" "); }
static void cleanup(u5c_component *c) { DBG(" "); }

/* The following fields are filled in dynamically:
 * name
 */
u5c_component random_comp = {
	.name = "random",
	.meta = "{ doc='a generator of random numbers', license='LGPL' }",
	.config = { .type = "struct rand_config", .data = &config },
	.ports = ports,
	.init = init,
	.exec = exec,
	.cleanup = cleanup,
};

static int random_init(void)
{
	DBG(" ");	
	/* return register_component(&random_comp); */
	return 0;
}

static void random_cleanup(void)
{
	DBG(" ");
	/* unregister_component(&random_comp); */
}

fblock_init(random_init)
fblock_cleanup(random_cleanup)
