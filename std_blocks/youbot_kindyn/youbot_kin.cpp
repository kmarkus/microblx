/*
 * youbot_kin microblx function block
 */

#include "ubx.h"

#define YOUBOT_NR_OF_JOINTS	5

/* Register a dummy type "struct youbot_kin" */
#include "types/youbot_kin.h"
#include "types/youbot_kin.h.hexarr"

#include <kdl/chain.hpp>
#include <kdl/frames.hpp>
#include <kdl/jntarrayvel.hpp>
#include <kdl/chainfksolvervel_recursive.hpp>
#include <kdl/chainiksolvervel_wdls.hpp>

using namespace KDL;

ubx_type_t types[] = {
	def_struct_type(struct youbot_kin, &youbot_kin_h),
	{ NULL },
};

/* block meta information */
char youbot_kin_meta[] =
	" { doc='',"
	"   license='',"
	"   real-time=true,"
	"}";

/* declaration of block configuration */
ubx_config_t youbot_kin_config[] = {
	{ .name="robot_model", .type_name = "char" },
	{ NULL },
};

/* declaration port block ports */
ubx_port_t youbot_kin_ports[] = {
	{ .name="arm_state", .in_type_name="struct motionctrl_jnt_state" },
	{ .name="arm_jnt_vel", .out_type_name="double", .out_data_len=YOUBOT_NR_OF_JOINTS },
	{ .name="arm_msr_twist", .out_type_name="struct kdl_twist" },
	{ .name="arm_cmd_twist", .out_type_name="struct kdl_twist" },
	{ NULL },
};

/* define a structure that contains the block state. By assigning an
 * instance of this struct to the block private_data pointer, this
 * struct is available the hook functions. (see init)
 */
struct youbot_kin_info
{
	Chain* chain;
	ChainFkSolverVel_recursive* fpk;	// forward position and velocity kinematics
	ChainIkSolverVel_wdls* ivk;		// Inverse velocity kinematic
};

/* declare convenience functions to read/write from the ports */
def_read_fun(read_uint, unsigned int)
def_write_fun(write_int, int)

/* init */
static int youbot_kin_init(ubx_block_t *b)
{
	struct youbot_kin_info* inf;
	int ret = -1;

	/* allocate memory for the block state */
	if ((b->private_data = calloc(1, sizeof(struct youbot_kin_info)))==NULL) {
		ERR("youbot_kin: failed to alloc memory");
		ret=EOUTOFMEM;
		goto out;
	}
	
	inf = (struct youbot_kin_info*) b->private_data;

	inf->chain = new Chain();

	/* youbot arm kinematics */
	inf->chain->addSegment(Segment(Joint(Joint::RotZ), Frame(Vector(0.024, 0.0, 0.096))));
	inf->chain->addSegment(Segment(Joint(Joint::RotY), Frame(Vector(0.033, 0.0, 0.019))));
	inf->chain->addSegment(Segment(Joint(Joint::RotY), Frame(Vector(0.000, 0.0, 0.155))));
	inf->chain->addSegment(Segment(Joint(Joint::RotY), Frame(Vector(0.000, 0.0, 0.135))));
	inf->chain->addSegment(Segment(Joint(Joint::RotZ), Frame(Vector(-0.002, 0.0, 0.130))));

	/* create solvers */
	inf->fpk = new ChainFkSolverVel_recursive(*inf->chain);
	inf->ivk = new ChainIkSolverVel_wdls(*inf->chain, 1.0);
	
	
	ret=0;
out:
	return ret;
}

/* start */
static int youbot_kin_start(ubx_block_t *b)
{
	int ret = 0;
	return ret;
}

/* stop */
static void youbot_kin_stop(ubx_block_t *b)
{
}

/* cleanup */
static void youbot_kin_cleanup(ubx_block_t *b)
{
	free(b->private_data);
}

/* step */
static void youbot_kin_step(ubx_block_t *b)
{
	unsigned int x;
	int y;

	ubx_port_t* foo = ubx_port_get(b, "foo");
	ubx_port_t* bar = ubx_port_get(b, "bar");

	/* read from a port */
	read_uint(foo, &x);
	y = x * -2;
	write_int(bar, &y);
}


/* put everything together */
ubx_block_t youbot_kin_block = {
	.name = "youbot_kin",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = youbot_kin_meta,
	.configs = youbot_kin_config,
	.ports = youbot_kin_ports,

	/* ops */
	.init = youbot_kin_init,
	.start = youbot_kin_start,
	.stop = youbot_kin_stop,
	.cleanup = youbot_kin_cleanup,
	.step = youbot_kin_step,
};


/* youbot_kin module init and cleanup functions */
static int youbot_kin_mod_init(ubx_node_info_t* ni)
{
	DBG(" ");
	int ret = -1;
	ubx_type_t *tptr;

	for(tptr=types; tptr->name!=NULL; tptr++) {
		if(ubx_type_register(ni, tptr) != 0) {
			goto out;
		}
	}

	if(ubx_block_register(ni, &youbot_kin_block) != 0)
		goto out;

	ret=0;
out:
	return ret;
}

static void youbot_kin_mod_cleanup(ubx_node_info_t *ni)
{
	DBG(" ");
	const ubx_type_t *tptr;

	for(tptr=types; tptr->name!=NULL; tptr++)
		ubx_type_unregister(ni, tptr->name);

	ubx_block_unregister(ni, "youbot_kin");
}

/* declare module init and cleanup functions, so that the ubx core can
 * find these when the module is loaded/unloaded */
UBX_MODULE_INIT(youbot_kin_mod_init)
UBX_MODULE_CLEANUP(youbot_kin_mod_cleanup)
