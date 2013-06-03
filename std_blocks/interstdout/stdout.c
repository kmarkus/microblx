/*
 * A fblock that generates stdout numbers.
 */

#define DEBUG 1

#include <stdio.h>
#include <stdlib.h>

#include "u5c.h"

char stdoutmeta[] =
	"{ doc='stdout interaction',"
	"  license='MIT',"
	"  real-time=false,"
	"}";

static int stdout_init(u5c_block_t *c)
{
	DBG(" ");
	return 0;
}

static int stdout_start(u5c_block_t *c)
{
	DBG("in");
	return 0; /* Ok */
}

static void stdout_stop(u5c_block_t *c) { DBG("in"); }

static void stdout_write(u5c_block_t *i, u5c_data_t* value) {
	DBG(" ");
}

static void stdout_cleanup(u5c_block_t *c) { DBG(" "); }

/* put everything together */
u5c_block_t stdout_comp = {
	.name = "stdout",
	.type = BLOCK_TYPE_INTERACTION,
	.meta_data = stdoutmeta,
	
	/* ops */
	.init = stdout_init,
	.start = stdout_start,
	.stop = stdout_stop,
	.cleanup = stdout_cleanup,
	.write=stdout_write,
};

static int stdout_mod_init(u5c_node_info_t* ni)
{
	DBG(" ");	
	return u5c_block_register(ni, &stdout_comp);
}

static void stdout_mod_cleanup(u5c_node_info_t *ni)
{
	DBG(" ");
	u5c_block_unregister(ni, BLOCK_TYPE_COMPUTATION, "stdout");
}

module_init(stdout_mod_init)
module_cleanup(stdout_mod_cleanup)
