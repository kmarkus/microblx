/*
 * A fblock that generates fmpc numbers.
 */

#define DEBUG 1

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "ubx.h"

#include "../youbot_driver/types/youbot_motorinfo.h"

/* function block meta-data
 * used by higher level functions.
 */
char fmpc_meta[] =
	"{ doc='Lin's fast MPC function block',"
	"  license='?',"
	"  real-time=true,"
	"}";


ubx_port_t fmpc_ports[] = {
	{ .name="cmd_vel", .attrs=PORT_DIR_OUT, .out_type_name="int32_t", .out_data_len=4 },
	{ .name="motor_info", .attrs=PORT_DIR_IN, .out_type_name="struct youbot_motorinfo" },
	{ NULL },
};

/* convenience functions to read/write from the ports */
def_write_arr_fun(write_int4, int32_t, 4)
def_read_fun(read_motorinfo, struct youbot_motorinfo)

/* your data here */
struct fmpc_info {
	int dummy;

};

static int fmpc_init(ubx_block_t *b)
{
	int ret=0;

	DBG(" ");

	if ((b->private_data = calloc(1, sizeof(struct fmpc_info)))==NULL) {
		ERR("Failed to alloc memory");
		ret=EOUTOFMEM;
		goto out;
	}
 out:
	return ret;
}


static void fmpc_cleanup(ubx_block_t *b)
{
	DBG(" ");
	free(b->private_data);
}

static int fmpc_start(ubx_block_t *b)
{
	DBG("in");
	return 0; /* Ok */
}

static void fmpc_step(ubx_block_t *b)
{
	int32_t cmd_vel[4];
	struct youbot_motorinfo ymi;

	struct fmpc_info* inf = (struct fmpc_info*) b->private_data;

	/* get ports */
	ubx_port_t* p_cmd_vel = ubx_port_get(b, "cmd_vel");
	ubx_port_t* p_motorinfo = ubx_port_get(b, "motor_info");

	/* read new motorinfo */
	read_motorinfo(p_motorinfo, & ymi);
	
	/* do something here */
	inf->dummy++;

	/* write out new velocity */
	write_int4(p_cmd_vel, &cmd_vel);
}


/* put everything together */
ubx_block_t fmpc_comp = {
	.name = "fmpc/fmpc",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = fmpc_meta,
	.ports = fmpc_ports,

	/* ops */
	.init = fmpc_init,
	.start = fmpc_start,
	.step = fmpc_step,
	.cleanup = fmpc_cleanup,
};

static int fmpc_module_init(ubx_node_info_t* ni)
{
	DBG(" ");
	return ubx_block_register(ni, &fmpc_comp);
}

static void fmpc_module_cleanup(ubx_node_info_t *ni)
{
	DBG(" ");
	ubx_block_unregister(ni, "fmpc/fmpc");
}

module_init(fmpc_module_init)
module_cleanup(fmpc_module_cleanup)
