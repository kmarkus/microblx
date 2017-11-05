/*
 * KUKA Youbot kinematics block.
 *
 * (C) 2011-2014 Markus Klotzbuecher <markus.klotzbuecher@mech.kuleuven.be>
 *     2010 Ruben Smits <ruben.smits@mech.kuleuven.be>
 *
 *            Department of Mechanical Engineering,
 *           Katholieke Universiteit Leuven, Belgium.
 *
 * see UBX_MODULE_LICENSE_SPDX tag below for licenses.
 */

// #define DEBUG	1

#include "ubx.h"

#define YOUBOT_NR_OF_JOINTS	5
#define IK_WDLS_LAMBDA		0.5


#include <kdl/chain.hpp>
#include <kdl/frames.hpp>
#include <kdl/jntarrayvel.hpp>
#include <kdl/chainfksolvervel_recursive.hpp>
#include <kdl/chainiksolvervel_wdls.hpp>

using namespace KDL;

/* from youbot_driver */
#include <motionctrl_jnt_state.h>

/* from std_types/kdl */
#include <kdl.h>


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
	/* FK */
	{ .name="arm_in_msr_pos", .in_type_name="double", .in_data_len=YOUBOT_NR_OF_JOINTS },
	{ .name="arm_in_msr_vel", .in_type_name="double", .in_data_len=YOUBOT_NR_OF_JOINTS },
	{ .name="arm_out_msr_ee_pose", .out_type_name="struct kdl_frame" },
	{ .name="arm_out_msr_ee_twist", .out_type_name="struct kdl_twist" },

	/* IK */
	{ .name="arm_in_cmd_ee_twist", .in_type_name="struct kdl_twist" },
	{ .name="arm_out_cmd_jnt_vel", .out_type_name="double", .out_data_len=YOUBOT_NR_OF_JOINTS },
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

	FrameVel *frame_vel;
	JntArrayVel *jnt_array;

	double cmd_jnt_vel[YOUBOT_NR_OF_JOINTS];

	ubx_port_t *p_arm_in_msr_pos;
	ubx_port_t *p_arm_in_msr_vel;
	ubx_port_t *p_arm_in_cmd_ee_twist;
	ubx_port_t *p_arm_out_cmd_jnt_vel;
	ubx_port_t *p_arm_out_msr_ee_pose;
	ubx_port_t *p_arm_out_msr_ee_twist;

};

/* declare convenience functions to read/write from the ports */
def_read_arr_fun(read_double5, double, YOUBOT_NR_OF_JOINTS)
def_read_fun(read_kdl_twist, struct kdl_twist)
def_write_arr_fun(write_jnt_arr, double, YOUBOT_NR_OF_JOINTS)
def_write_fun(write_kdl_frame, struct kdl_frame)
def_write_fun(write_kdl_twist, struct kdl_twist)

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

	/* youbot arm kinematics, derived from:
	 * https://github.com/kmarkus/youbot-ros-pkg/tree/master/youbot_common/youbot_description/urdf/youbot_arm
	 */
	/* arm_link_1 (Joint constructor is (origin, axis, type) ) */
	inf->chain->addSegment( Segment( Joint( Vector(0.024, 0.0, 0.096), Vector(0.0, 0.0, 1.0), Joint::RotAxis ),
					 Frame( Vector(0.024, 0.0, 0.096) )));

	/* arm_link_2 */
	inf->chain->addSegment( Segment( Joint( Vector(0.033, 0.0, 0.019), Vector(0.0, -1.0, 0.0), Joint::RotAxis ),
					 Frame( Vector(0.033, 0.0, 0.019) )));

	/* arm_link_3 */
	inf->chain->addSegment( Segment( Joint( Vector(0.0, 0.0, 0.155), Vector(0.0, 1.0, 0.0), Joint::RotAxis ),
					 Frame( Vector(0.0, 0.0, 0.155) )));

	/* arm_link_4 */
	inf->chain->addSegment( Segment( Joint( Vector(0.0, 0.0, 0.135), Vector(0.0, -1, 0.0), Joint::RotAxis),
					 Frame( Vector(0.0, 0.0, 0.135) )));

	/* arm_link_5 */
	inf->chain->addSegment( Segment( Joint( Vector(-0.002, 0.0, 0.13), Vector(0.0, 0.0, 1.0), Joint::RotAxis),
					 Frame( Vector(-0.002, 0.0, 0.13) )));

	/* create and configure solvers */
	inf->fpk = new ChainFkSolverVel_recursive(*inf->chain);
	inf->ivk = new ChainIkSolverVel_wdls(*inf->chain, 1.0);

	inf->ivk->setLambda(IK_WDLS_LAMBDA);

	/* extra data */
	inf->frame_vel = new FrameVel();
	inf->jnt_array = new JntArrayVel(YOUBOT_NR_OF_JOINTS);

	/* cache port ptrs */
	assert(inf->p_arm_in_msr_pos = ubx_port_get(b, "arm_in_msr_pos"));
	assert(inf->p_arm_in_msr_vel = ubx_port_get(b, "arm_in_msr_vel"));
	assert(inf->p_arm_in_cmd_ee_twist = ubx_port_get(b, "arm_in_cmd_ee_twist"));
	assert(inf->p_arm_out_cmd_jnt_vel = ubx_port_get(b, "arm_out_cmd_jnt_vel"));
	assert(inf->p_arm_out_msr_ee_pose = ubx_port_get(b, "arm_out_msr_ee_pose"));
	assert(inf->p_arm_out_msr_ee_twist = ubx_port_get(b, "arm_out_msr_ee_twist"));

	ret=0;
out:
	return ret;
}

/* start */
static int youbot_kin_start(ubx_block_t *b)
{
	return 0;
}

/* stop */
static void youbot_kin_stop(ubx_block_t *b)
{
}

/* cleanup */
static void youbot_kin_cleanup(ubx_block_t *b)
{
	struct youbot_kin_info* inf;
	inf = (struct youbot_kin_info*) b->private_data;

	delete inf->frame_vel;
	delete inf->jnt_array;
	delete inf->fpk;
	delete inf->ivk;
	delete inf->chain;
	free(b->private_data);
}

/* step */
static void youbot_kin_step(ubx_block_t *b)
{
	int ret;
	struct kdl_twist ee_twist;
	// double jnt_vel[YOUBOT_NR_OF_JOINTS] = { 0, 0, 0, 0, 0};
	double msr_pos[YOUBOT_NR_OF_JOINTS] = { 0, 0, 0, 0, 0};
	double msr_vel[YOUBOT_NR_OF_JOINTS] = { 0, 0, 0, 0, 0};

	Twist const *KDLTwistPtr;
	Frame KDLPose;
	Twist KDLTwist;

	struct youbot_kin_info* inf;
	inf = (struct youbot_kin_info*) b->private_data;

	/* read jnt state and compute forward kinematics */
	if(read_double5(inf->p_arm_in_msr_pos, &msr_pos) == 5 &&
	   read_double5(inf->p_arm_in_msr_vel, &msr_vel) == 5) {

		DBG("computing FK");
		DBG("msr_pos: %3f %3f %3f %3f %3f", msr_pos[0], msr_pos[1], msr_pos[2], msr_pos[3], msr_pos[4]);

		for(int i=0;i<YOUBOT_NR_OF_JOINTS;i++) {
			inf->jnt_array->q(i) = msr_pos[i];
			inf->jnt_array->qdot(i) = msr_vel[i];
		}

		/* compute and write out current EE Pose and Twist */
		inf->fpk->JntToCart(*inf->jnt_array, *inf->frame_vel);

		KDLPose = inf->frame_vel->GetFrame();
		KDLTwist = inf->frame_vel->GetTwist();

		DBG("KDLPose:\n%3f %3f %3f %3f\n%3f %3f %3f %3f\n%3f %3f %3f %3f\n%3f %3f %3f %3f",
		    KDLPose.M.data[0], KDLPose.M.data[1], KDLPose.M.data[2], KDLPose.p[0],
		    KDLPose.M.data[3], KDLPose.M.data[4], KDLPose.M.data[5], KDLPose.p[1],
		    KDLPose.M.data[6], KDLPose.M.data[7], KDLPose.M.data[8], KDLPose.p[2],
		    0.0, 0.0, 0.0, 1.0);


		write_kdl_frame(inf->p_arm_out_msr_ee_pose, (struct kdl_frame*) &KDLPose);
		write_kdl_twist(inf->p_arm_out_msr_ee_twist, (struct kdl_twist*) &KDLTwist);

	}

	/* read cmd_ee_twist and compute inverse kinematics */
	if(read_kdl_twist(inf->p_arm_in_cmd_ee_twist, &ee_twist) == 1) {

		DBG("computing IK");

		KDLTwistPtr = (Twist*) &ee_twist; /* uh */
		// kdl_twist = reinterpret_cast<Twist*>(&ee_twist);
		ret = inf->ivk->CartToJnt(inf->jnt_array->q, *KDLTwistPtr, inf->jnt_array->qdot);

		if(ret >= 0) {
			for(int i=0;i<YOUBOT_NR_OF_JOINTS;i++)
				inf->cmd_jnt_vel[i] = inf->jnt_array->qdot(i);
		} else {
			ERR("%s: Failed to compute inverse velocity kinematics", b->name);
			/* set jnt_vel to zero (jnt_vel is initialized to zero) */
		}
	}
	/* write out desired jnt velocities */
	write_jnt_arr(inf->p_arm_out_cmd_jnt_vel, &inf->cmd_jnt_vel);
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

	if(ubx_block_register(ni, &youbot_kin_block) != 0)
		goto out;

	ret=0;
out:
	return ret;
}

static void youbot_kin_mod_cleanup(ubx_node_info_t *ni)
{
	DBG(" ");
	ubx_block_unregister(ni, "youbot_kin");
}

/* declare module init and cleanup functions, so that the ubx core can
 * find these when the module is loaded/unloaded */
UBX_MODULE_INIT(youbot_kin_mod_init)
UBX_MODULE_CLEANUP(youbot_kin_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause LGPL-2.1+)
