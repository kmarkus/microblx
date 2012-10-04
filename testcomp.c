/*
 * A fblock that generates random numbers.
 */
#include <stdio.h>
#include "u5c.h"

#ifdef DEBUG
# define DBG(fmt, args...) ( fprintf(stderr, "%s: ", __FUNCTION__), \
			     fprintf(stderr, fmt, ##args),	    \
			     fprintf(stderr, "\n") )
#else
# define DBG(fmt, args...)  do {} while (0)
#endif

static int init(u5c_component *c)
{
	DBG("");
	return 0;
}

static void exec(u5c_component *c)
{
	DBG("");
}

static void cleanup(u5c_component *c)
{
	DBG("");
}

u5c_port ports[] = { 
	{ .name="random", .direction=PORT_DIR_OUT, .data_type="kdl_twist" }, 
	{ .name="seed", .direction=PORT_DIR_IN, .data_type="unsigned int" },
	{ NULL },
};

u5c_component comp = {
	.name = "rand",
	.doc = "a generator of random numbers",
	.ports = ports,
	.init = init,
	.exec = exec,
	.cleanup = cleanup,
};
