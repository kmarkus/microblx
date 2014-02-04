/*
 * KUKA Youbot microblx driver.
 *
 * (C) 2011-2014 Markus Klotzbuecher <markus.klotzbuecher@mech.kuleuven.be>
 *     2010 Ruben Smits <ruben.smits@mech.kuleuven.be>
 *     2010 Steven Bellens <steven.bellens@mech.kuleuven.be>
 *
 *            Department of Mechanical Engineering,
 *           Katholieke Universiteit Leuven, Belgium.
 *
 * see UBX_MODULE_LICENSE_SPDX tag below for licenses.
 */

#define DEBUG 1

/* SOEM stuff */
#include "ethercattype.h"
#include "nicdrv.h"
#include "ethercatbase.h"
#include "ethercatmain.h"
#include "ethercatconfig.h"
#include "ethercatcoe.h"
#include "ethercatdc.h"
#include "ethercatprint.h"

#include <time.h>

#include "ubx.h"

#include "youbot_driver.h"

char youbot_meta[] =
	"{ doc='youbot driver block',"
	"  license={ 'LGPLv2.1', 'ModifedBSD' } "
	"  real-time=true,"
	"}";


ubx_config_t youbot_config[] = {
	{ .name="ethernet_if", .type_name = "char" },
	{ .name="nr_arms", .type_name = "char", .attrs=CONFIG_ATTR_RDONLY },
	{ NULL },
};

ubx_port_t youbot_ports[] = {
	/* generic arm+base */
	{ .name="events", .in_type_name="unsigned int", .doc="outgoing coordination events" },

	/* base */
	{ .name="base_control_mode", .in_type_name="int32_t", .out_type_name="int32_t", .doc="desired base control mode" },
	{ .name="base_cmd_twist", .in_type_name="struct kdl_twist", .doc="desired twist motion of base [m/s, rad/s]" },
	{ .name="base_cmd_vel", .in_type_name="int32_t", .in_data_len=YOUBOT_NR_OF_WHEELS, .doc="desired joint space base velocity [rpm]" },
	{ .name="base_cmd_cur", .in_type_name="int32_t", .in_data_len=YOUBOT_NR_OF_WHEELS, .doc="desired current to apply to joints [mA]" },

	{ .name="base_msr_odom",  .out_type_name="struct kdl_frame", .doc="odometry information" },
	{ .name="base_msr_twist", .out_type_name="struct kdl_twist", .doc="current, measured twist" },
	{ .name="base_motorinfo", .out_type_name="struct youbot_base_motorinfo", .doc="low-level motor information" },

	/* arm */
	{ .name="arm1_control_mode", .in_type_name="int32_t", .out_type_name="int32_t", .doc="desired arm control mode" },
	{ .name="arm1_calibrate_cmd", .in_type_name="int32_t", .doc="writing any value will start arm calibration" },
	{ .name="arm1_cmd_pos", .in_type_name="double", .in_data_len=YOUBOT_NR_OF_JOINTS, .doc="desire joint space arm position [rad]" },
	{ .name="arm1_cmd_vel", .in_type_name="double", .in_data_len=YOUBOT_NR_OF_JOINTS, .doc="desired joint space arm velocity [rad/s]" },
	{ .name="arm1_cmd_eff", .in_type_name="double", .in_data_len=YOUBOT_NR_OF_JOINTS, .doc="desired effort to apply to joints" },
	{ .name="arm1_cmd_cur", .in_type_name="int32_t", .in_data_len=YOUBOT_NR_OF_JOINTS, .doc="desired current to apply to joints [mA]" },

	{ .name="arm1_motorinfo",  .out_type_name="struct youbot_arm_motorinfo", .doc="low-level motor information" },
#ifdef USE_ARM_JNT_STATE_STRUCT
	{ .name="arm1_state", .out_type_name="struct motionctrl_jnt_state", .doc="struct containing arrays[5] for pos, vel and eff" },
#else
	{ .name="arm1_msr_pos", .out_type_name="double", .out_data_len=YOUBOT_NR_OF_JOINTS, .doc="measured arm position joint space [rad]" },
	{ .name="arm1_msr_vel", .out_type_name="double", .out_data_len=YOUBOT_NR_OF_JOINTS, .doc="measured arm velocity joint space [rad/s]" },
	{ .name="arm1_msr_eff", .out_type_name="double", .out_data_len=YOUBOT_NR_OF_JOINTS, .doc="measured arm effort joint space [N/m]" },
#endif
	{ .name="arm1_gripper", .in_type_name="int32_t", .out_type_name="int32_t", .doc="gripper control port, 1 to open, 0 to close" },

	{ NULL },
};

/* types */
/* declare types */

#include "types/youbot_base_motorinfo.h"
#include "types/youbot_base_motorinfo.h.hexarr"

#ifdef USE_ARM_JNT_STATE_STRUCT
/* high level: m, m/s, Nm */
#include "types/motionctrl_jnt_state.h.hexarr"
#endif

/* low level: ticks, rpm, current */
#include "types/youbot_arm_motorinfo.h"
#include "types/youbot_arm_motorinfo.h.hexarr"

#include "types/youbot_control_modes.h.hexarr"

ubx_type_t youbot_types[] = {
	def_struct_type(struct youbot_base_motorinfo, &youbot_base_motorinfo_h),
	def_struct_type(struct youbot_arm_motorinfo, &youbot_arm_motorinfo_h),

#ifdef USE_ARM_JNT_STATE_STRUCT
	def_struct_type(struct motionctrl_jnt_state, &motionctrl_jnt_state_h),
#endif
	/* def_struct_type("youbot", enum youbot_control_modes, &youbot_control_modes_h), */
	{ NULL },
};



/*******************************************************************************
 *									       *
 *                         generic arm+base driver code			       *
 *									       *
 *******************************************************************************/


/* convenience functions to read/write from the ports */
def_read_fun(read_int, int32_t)
def_write_fun(write_int, int32_t)

def_read_arr_fun(read_int4, int32_t, 4)
def_read_arr_fun(read_int5, int32_t, 5)
def_read_arr_fun(read_double5, double, 5)

def_read_fun(read_kdl_twist, struct kdl_twist)
def_write_fun(write_kdl_twist, struct kdl_twist)

def_write_fun(write_kdl_frame, struct kdl_frame)
def_write_fun(write_base_motorinfo, struct youbot_base_motorinfo)

def_write_fun(write_arm_motorinfo, struct youbot_arm_motorinfo)

#ifdef USE_ARM_JNT_STATE_STRUCT
def_write_fun(write_arm_state, struct motionctrl_jnt_state)
#else
def_write_arr_fun(write_double5, double, 5)
#endif

/**
 * validate_base_slaves - check whether a valid base was detected.
 *
 * Compare the slave names starting from slave start_num in the global
 * ec_slave datastructure with those expeceted for a youbot base.
 *
 * @param start_num starting index in ec_slave array
 *
 * @return 0 if base is valid, -1 otherwise.
 */
static int validate_base_slaves(int start_num)
{
	int ret=-1, i=start_num;

	if(strcmp(ec_slave[i].name, YOUBOT_WHEEL_PWRB_NAME) != 0) {
		ERR("expected base powerboard (%s), found %s",
		    YOUBOT_WHEEL_PWRB_NAME, ec_slave[1].name);
		goto out;
	}

	for(i++; i<=start_num+YOUBOT_NR_OF_WHEELS; i++) {
		if(strcmp(ec_slave[i].name, YOUBOT_WHEEL_CTRL_NAME) == 0 &&
		   strcmp(ec_slave[i].name, YOUBOT_WHEEL_CTRL_NAME2) == 0) {
			ERR("expected base controller (%s or %s), found %s",
			    YOUBOT_WHEEL_CTRL_NAME, YOUBOT_WHEEL_CTRL_NAME2, ec_slave[i].name);
			goto out;
		}
	}
	ret=0;
 out:
	return ret;
}

/**
 * validate_arm_slaves - check whether a valid arm was detected.
 *
 * Compare the slave names starting from slave start_num in the global
 * ec_slave datastructure with those expeceted for a youbot arm.
 *
 * @param start_num starting index in ec_slave array
 *
 * @return 0 if arm is valid, -1 otherwise.
 */
static int validate_arm_slaves(int start_num)
{
	int ret=-1, i=start_num;

	if(strcmp(ec_slave[i].name, YOUBOT_ARM_PWRB_NAME) != 0) {
		ERR("expected arm powerboard (%s), found %s",
		    YOUBOT_ARM_PWRB_NAME, ec_slave[1].name);
		goto out;
	}

	for(i++; i<=start_num+YOUBOT_NR_OF_JOINTS; i++) {
		if(strcmp(ec_slave[i].name, YOUBOT_ARM_CTRL_NAME) == 0 &&
		   strcmp(ec_slave[i].name, YOUBOT_ARM_CTRL_NAME2) == 0) {
			ERR("expected arm controller (%s or %s), found %s",
			    YOUBOT_ARM_CTRL_NAME, YOUBOT_ARM_CTRL_NAME2, ec_slave[i].name);
			goto out;
		}
	}
	ret=0;
 out:
	return ret;
}

/**
 * send_mbx send a message to a youbot motor slave.
 *
 * @param gripper 1 to address the gripper, 0 otherwise
 * @param instr_nr command number (GAP, SAP, ...)
 * @param param_nr type number
 * @param slave_nr slave index
 * @param bank_nr mostly zero. Motor or Bank number
 * @param value value to send and location to store result.
 *
 * @return 0 if Ok, -1 otherwise.
 */
int send_mbx(int gripper,
	     uint8_t instr_nr,
	     uint8_t param_nr,
	     uint16_t slave_nr,
	     uint8_t bank_nr,
	     int32_t *value)
{
	int ret=-1;
	char* msg;
	ec_mbxbuft mbx_out, mbx_in;

	mbx_out[0] = (gripper) ? 1 : 0;
	mbx_out[1] = instr_nr;
	mbx_out[2] = param_nr;
	mbx_out[3] = bank_nr;

	/* TODO: replace with htobe32 */
	mbx_out[4] = (uint32)(*value >> 24);
	mbx_out[5] = (uint32)(*value >> 16);
	mbx_out[6] = (uint32)(*value >> 8);
	mbx_out[7] = (uint32)(*value & 0xff);

	/* DBG("sending send mbx (gripper:%d, instr_nr=%d, param_nr=%d, slave_nr=%d, bank_nr=%d, value=%d", */
	/*     gripper, instr_nr, param_nr, slave_nr, bank_nr, *value); */

	if (ec_mbxsend(slave_nr, &mbx_out, EC_TIMEOUTTXM) <= 0) {
		ERR("failed to send mbx (gripper:%d, instr_nr=%d, param_nr=%d, slave_nr=%d, bank_nr=%d, value=%d",
		    gripper, instr_nr, param_nr, slave_nr, bank_nr, *value);
		goto out;
	}

	if (ec_mbxreceive(slave_nr, &mbx_in, EC_TIMEOUTRXM) <= 0) {
		ERR("failed to receive mbx (gripper:%d, instr_nr=%d, param_nr=%d, slave_nr=%d, bank_nr=%d, value=%d",
		    gripper, instr_nr, param_nr, slave_nr, bank_nr, *value);
		goto out;
	}

	*value = (mbx_in[4] << 24 | mbx_in[5] << 16 | mbx_in[6] << 8 | mbx_in[7]);

	switch(((int) mbx_in[2])) {
	case YB_MBX_STAT_OK: goto out_ok;
	case YB_MBX_STAT_INVALID_CMD: msg="invalid command"; break;
	case YB_MBX_STAT_WRONG_TYPE: msg="wrong type"; break;
	case YB_MBX_STAT_EINVAL: msg="invalid value"; break;
	case YB_MBX_STAT_EEPROM_LOCKED: msg="Configuration EEPROM locked"; break;
	case YB_MBX_STAT_CMD_UNAVAILABLE: msg="Command not available"; break;
	case YB_MBX_STAT_PARAM_PWDPROT: msg="Parameter is password protected"; break;
	default: msg="unkown error";
	}

	if(((int) mbx_in[2]) != 100) {
		ERR("receiving mbx failed: module=%d, status=%d, command=%d, value=%d: %s.",
		    mbx_in[1], mbx_in[2], mbx_in[3], *value, msg);
		goto out;
	}
out_ok:
	ret=0;
out:
	return ret;
}

/**
 * set slave parameter on one or more slaves.
 *
 * @param slave_start
 * @param num_slaves number of consecutive slaves starting from slave_start to set param on.
 * @param nr parameter number
 * @param val value to set parameter to.
 *
 * @return
 */
int set_param(uint32_t slave_nr, uint8_t param_nr, int32_t val)
{
	int32_t ret=-1, rdval;

	DBG("set_param: setting slave %d param_nr=%d to val=%d", slave_nr, param_nr, val);

	/* read param */
	if(send_mbx(0, GAP, param_nr, slave_nr, 0, &rdval)) {
		ERR("slave %d, failed to read param=%d", slave_nr, param_nr);
		goto out;
	}

	if (rdval != val)
		DBG("set_param: slave=%d, param=%d not yet set, trying to set.", slave_nr, param_nr);

	/* set param */
	if(send_mbx(0, SAP, param_nr, slave_nr, 0, &val)) {
		ERR("failed to set slave=%d param=%d to val=%d. Password required?", slave_nr, param_nr, val);
		goto out;
	}

	// re-read and check
	if(send_mbx(0, GAP, param_nr, slave_nr, 0, &rdval)) {
		ERR("slave %d, failed to re-read param=%d", slave_nr, param_nr);
		goto out;
	}

	if (rdval != val) {
		ERR("setting param failed.");
		goto out;
	}

	ret=0;
 out:
	return ret;
}


/*******************************************************************************
 *									       *
 *                            youbot base driver code			       *
 *									       *
 *******************************************************************************/

static int base_config_params(struct youbot_base_info* binf)
{
	binf->control_mode = YOUBOT_CMODE_INITIALIZE;

#ifndef FIRMWARE_V2
	int i;
	for(i=0; i<YOUBOT_NR_OF_WHEELS; i++) {
		set_param(binf->wheel_inf[i].slave_idx, COMMUTATION_MODE, 3);
		set_param(binf->wheel_inf[i].slave_idx, 167, 0); // Block PWM scheme
		// set_param(binf->wheel_inf[i].slave_idx,  240, ?); // Block PWM scheme
		set_param(binf->wheel_inf[i].slave_idx, 249, 1); // Block commutation init using hall sensors
		set_param(binf->wheel_inf[i].slave_idx, 250, 4000); // nr encoder steps per rotation
		set_param(binf->wheel_inf[i].slave_idx, 251, 1); // or 0?
		set_param(binf->wheel_inf[i].slave_idx, 253, 16); // nr motor poles
		set_param(binf->wheel_inf[i].slave_idx, 254, 1); // invert hall sensor
	}
#endif
	return 0;
}

static int arm_config_params(struct youbot_arm_info* ainf)
{
	ainf->control_mode = YOUBOT_CMODE_INITIALIZE;

#ifndef FIRMWARE_V2
	int32_t val;
	int i, ret=-1;

	for(i=0; i<YOUBOT_NR_OF_JOINTS; i++) {
		set_param(ainf->jnt_inf[i].slave_idx, COMMUTATION_MODE, 3);
		set_param(ainf->jnt_inf[i].slave_idx, 167, 0); // Block PWM scheme
		// set_param(ainf->jnt_inf[i].slave_idx, 240, ?); // Block PWM scheme
		set_param(ainf->jnt_inf[i].slave_idx, 249, 1); // Block commutation init using hall sensors
		set_param(ainf->jnt_inf[i].slave_idx, 250, 4000); // nr encoder steps per rotation
		set_param(ainf->jnt_inf[i].slave_idx, 251, 1); // or 0?
		set_param(ainf->jnt_inf[i].slave_idx, 253, 16); // nr motor poles
		set_param(ainf->jnt_inf[i].slave_idx, 254, 1); // invert hall sensor
	}
	return ret;
#else /* FIRMWARE_V2 */
	return 0;
#endif /* FIRMWARE_V2 */
}


/* set all commanded quantities to zero */
static int base_prepare_start(struct youbot_base_info *base)
{
	int i, ret=-1;
	int32_t dummy;

	for(i=0; i<YOUBOT_NR_OF_WHEELS; i++)
		base->wheel_inf[i].cmd_val=0;

	/* reset EC_TIMEOUT */
	for(i=0; i<YOUBOT_NR_OF_WHEELS; i++) {
		if(send_mbx(0, SAP, CLR_EC_TIMEOUT, base->wheel_inf[i].slave_idx, 0, &dummy)) {
			ERR("failed to clear EC_TIMEOUT");
			goto out;
		}
	}

	ret=0;
 out:
	return ret;
}

#if 0
static int32_t arm_is_calibrated();
#endif

/* set all commanded quantities to zero */
static int arm_prepare_start(struct youbot_arm_info *arm)
{
	int i, ret=-1;
	int32_t val;

	for(i=0; i<YOUBOT_NR_OF_JOINTS; i++)
		arm->jnt_inf[i].cmd_val=0;

#ifdef LIMIT_CURRENT_SOFT
	for(i=0; i<YOUBOT_NR_OF_JOINTS; i++) {
		if(send_mbx(0, GAP, MAX_CURRENT, arm->jnt_inf[i].slave_idx, 0, &val)) {
			ERR("failed to get MAX_CURRENT for slave %d", arm->jnt_inf[i].slave_idx);
			goto out;
		}
		arm->jnt_inf[i].max_cur = val;
		DBG("limiting axis[%d] to max_current %d", i, val);
	}
#endif

#if 0
	DBG("arm calibrated: %d", arm_is_calibrated());
#endif

	/* reset EC_TIMEOUT */
	for(i=0; i<YOUBOT_NR_OF_JOINTS; i++) {
		if(send_mbx(0, SAP, CLR_EC_TIMEOUT, arm->jnt_inf[i].slave_idx, 0, &val)) {
			ERR("failed to clear EC_TIMEOUT");
			goto out;
		}
	}

	ret=0;
 out:
	return ret;
}



/**
 * base_proc_errflg - update statistics and return number of fatal
 * errors.
 *
 * @param binf
 *
 * @return number of fatal errors
 */
static int base_proc_errflg(struct youbot_base_info* binf)
{
	int fatal_errs=0, i, dummy;

	/* check and raise errors */
	for(i=0; i<YOUBOT_NR_OF_WHEELS; i++) {
		if(binf->wheel_inf[i].error_flags & OVERCURRENT) {
			binf->wheel_inf[i].stats.overcurrent++; fatal_errs++;
			DBG("OVERCURRENT on wheel %i", i);
		}
		if(binf->wheel_inf[i].error_flags & UNDERVOLTAGE) {
			binf->wheel_inf[i].stats.undervoltage++; fatal_errs++;
			DBG("UNDERVOLTAGE on wheel %i", i);
		}
		if(binf->wheel_inf[i].error_flags & OVERTEMP) {
			binf->wheel_inf[i].stats.overtemp++; fatal_errs++;
			DBG("OVERTEMP on wheel %i", i);
		}
		if(binf->wheel_inf[i].error_flags & HALL_ERR) {
			binf->wheel_inf[i].stats.hall_err++;
			DBG("HALL_ERR on wheel %i", i);
		}
		if(binf->wheel_inf[i].error_flags & ENCODER_ERR) {
			binf->wheel_inf[i].stats.encoder_err++;
			DBG("ENCODER_ERR on wheel %i", i);
		}
		if(binf->wheel_inf[i].error_flags & SINE_COMM_INIT_ERR) {
			binf->wheel_inf[i].stats.sine_comm_init_err++;
			DBG("SINE_COMM_INIT_ERR on wheel %i", i);
		}
		if(binf->wheel_inf[i].error_flags & EMERGENCY_STOP) {
			binf->wheel_inf[i].stats.emergency_stop++;
			DBG("EMERGENCY_STOP on wheel %i", i);
		}
		if(binf->wheel_inf[i].error_flags & MODULE_INIT) {
			/* if (! (binf->mod_init_stat & (1 << i))) DBG("MODULE_INIT set for wheel %d", i); */
			binf->mod_init_stat |= 1<<i;
		}
		if(binf->wheel_inf[i].error_flags & EC_TIMEOUT) {
			binf->wheel_inf[i].stats.ec_timeout++;
			DBG("EC_TIMEOUT on wheel %i", i);
			if(send_mbx(0, SAP, CLR_EC_TIMEOUT, binf->wheel_inf[i].slave_idx, 0, &dummy)) {
				ERR("failed to clear EC_TIMEOUT flag");
				fatal_errs++;
			}
		}

		if(binf->wheel_inf[i].error_flags & I2T_EXCEEDED && !binf->wheel_inf[i].i2t_ex) {
			/* i2t error rising edge */
			ERR("I2T_EXCEEDED on wheel %i setting base to control_mode=MotorStop.", i);
			binf->wheel_inf[i].stats.i2t_exceeded++;
			binf->wheel_inf[i].i2t_ex=1;
			binf->control_mode=YOUBOT_CMODE_MOTORSTOP;
			fatal_errs++;

			if(send_mbx(0, SAP, CLEAR_I2T, binf->wheel_inf[i].slave_idx, 0, &dummy))
				ERR("wheel %d, failed to clear I2T_EXCEEDED bit", i);

		} else if (!(binf->wheel_inf[i].error_flags & I2T_EXCEEDED) && binf->wheel_inf[i].i2t_ex) {
			/* i2t error falling edge */
			DBG("I2T_EXCEEDED reset on wheel %i", i);
			binf->wheel_inf[i].i2t_ex=0;
		}
	}

	return fatal_errs;
}

static void base_output_msr_data(struct youbot_base_info* base)
{
	int i;
	float delta_pos[YOUBOT_NR_OF_WHEELS];
	struct youbot_base_motorinfo motorinf;


	/* copy and write out the "raw" driver information */
	for(i=0; i<YOUBOT_NR_OF_WHEELS; i++) {
		motorinf.pos[i]=base->wheel_inf[i].msr_pos;
		motorinf.vel[i]=base->wheel_inf[i].msr_vel;
		motorinf.cur[i]=base->wheel_inf[i].msr_cur;
	}

	write_base_motorinfo(base->p_base_motorinfo, &motorinf);


	/* translate wheel velocities to cartesian base velocities */
	base->msr_twist.vel.x =
		(double) ( base->wheel_inf[0].msr_vel - base->wheel_inf[1].msr_vel
			   + base->wheel_inf[2].msr_vel - base->wheel_inf[3].msr_vel )
		/ (double) ( YOUBOT_NR_OF_WHEELS *  YOUBOT_CARTESIAN_VELOCITY_TO_RPM );

	base->msr_twist.vel.y =
		(double) ( - base->wheel_inf[0].msr_vel - base->wheel_inf[1].msr_vel
			   + base->wheel_inf[2].msr_vel + base->wheel_inf[3].msr_vel )
		/ (double) ( YOUBOT_NR_OF_WHEELS * YOUBOT_CARTESIAN_VELOCITY_TO_RPM );

	base->msr_twist.rot.z =
		(double) ( - base->wheel_inf[0].msr_vel - base->wheel_inf[1].msr_vel
			   - base->wheel_inf[2].msr_vel - base->wheel_inf[3].msr_vel )
		/ (double) ( YOUBOT_NR_OF_WHEELS * YOUBOT_CARTESIAN_VELOCITY_TO_RPM * YOUBOT_ANGULAR_TO_WHEEL_VELOCITY);

	write_kdl_twist(base->p_msr_twist, &base->msr_twist);


	/* TODO: this odometry calculation is broken. It goes haywire
	 * upon rotating the base */

	/* skip the first ten samples.
	 * mk: I think we can remove this?
	 */
	if(base->odom_started < 10) {
		for(i=0; i<YOUBOT_NR_OF_WHEELS; i++)
			base->last_pos[i]=base->wheel_inf[i].msr_pos;
		base->odom_started++;
		goto out;
	}

	for(i = 0; i < YOUBOT_NR_OF_WHEELS; i++) {
		delta_pos[i] =
			(float) ( base->wheel_inf[i].msr_pos - base->last_pos[i] )
			* (float) (YOUBOT_WHEEL_CIRCUMFERENCE)
			/ (float) (YOUBOT_TICKS_PER_REVOLUTION * YOUBOT_MOTORTRANSMISSION ) ;
		base->last_pos[i] = base->wheel_inf[i].msr_pos;
	}

	base->odom_yaw += ( delta_pos[0] + delta_pos[1] + delta_pos[2] + delta_pos[3] )
		/ ( YOUBOT_NR_OF_WHEELS * YOUBOT_ANGULAR_TO_WHEEL_VELOCITY );

	base->msr_odom.p.x +=
		- ( ( ( delta_pos[0] - delta_pos[1] + delta_pos[2] - delta_pos[3] )
		      / YOUBOT_NR_OF_WHEELS ) * cos( base->odom_yaw )
		    - ( ( - delta_pos[0] - delta_pos[1] + delta_pos[2] + delta_pos[3] )
			/ YOUBOT_NR_OF_WHEELS ) * sin( base->odom_yaw ));

	base->msr_odom.p.y +=
		- ( ( ( delta_pos[0] - delta_pos[1] + delta_pos[2] - delta_pos[3] )
		      / YOUBOT_NR_OF_WHEELS ) * sin( base->odom_yaw )
		    + ( ( - delta_pos[0] - delta_pos[1] + delta_pos[2] + delta_pos[3] )
			/ YOUBOT_NR_OF_WHEELS ) * cos( base->odom_yaw ));

	/* m_odom.pose.pose.orientation = tf::createQuaternionMsgFromYaw(base->odom_yaw); */
	write_kdl_frame(base->p_msr_odom, &base->msr_odom);


 out:
	return;
}


/**
 * twist_to_wheel_val - convert twist to wheel motions.
 * More precisely: convert base->command_twist to base->wheel_inf[i].cmd_val.
 *
 * @param base
 */
void twist_to_wheel_val(struct youbot_base_info* base)
{
	struct kdl_twist* twist=&base->cmd_twist;

	base->wheel_inf[0].cmd_val = ( -twist->vel.x + twist->vel.y + twist->rot.z *
				       YOUBOT_ANGULAR_TO_WHEEL_VELOCITY ) * YOUBOT_CARTESIAN_VELOCITY_TO_RPM;

	base->wheel_inf[1].cmd_val = ( twist->vel.x + twist->vel.y + twist->rot.z *
				       YOUBOT_ANGULAR_TO_WHEEL_VELOCITY ) * YOUBOT_CARTESIAN_VELOCITY_TO_RPM;

	base->wheel_inf[2].cmd_val = ( -twist->vel.x - twist->vel.y + twist->rot.z  *
				       YOUBOT_ANGULAR_TO_WHEEL_VELOCITY ) * YOUBOT_CARTESIAN_VELOCITY_TO_RPM;

	base->wheel_inf[3].cmd_val = ( twist->vel.x - twist->vel.y + twist->rot.z *
				       YOUBOT_ANGULAR_TO_WHEEL_VELOCITY ) * YOUBOT_CARTESIAN_VELOCITY_TO_RPM;
}

/**
 * base_is_configured - check if all base slaves have been configured.
 *
 * @param base
 *
 * @return 1 if configured, 0 otherwise.
 */
static int base_is_configured(struct youbot_base_info* base)
{
	return (base->mod_init_stat == ((1 << YOUBOT_NR_OF_WHEELS)-1));
}

/**
 * base_check_watchdog - check if expired or trigger.
 * currently only second resolution!
 *
 * @param base
 * @param trig if != 0 then trigger watchdog
 */
static void base_check_watchdog(struct youbot_base_info* base, int trig)
{
	struct timespec ts_cur;

	if(clock_gettime(CLOCK_MONOTONIC, &ts_cur) != 0) {
		ERR2(errno, "clock_gettime failed");
		goto out;
	}

	if(trig) {
		base->last_cmd = ts_cur;
		goto out;
	}

	if(ts_cur.tv_sec - base->last_cmd.tv_sec >= BASE_TIMEOUT) {
		ERR("watchdog timeout out, setting control_mode to MOTORSTOP");
		base->control_mode=YOUBOT_CMODE_MOTORSTOP;
		goto out;
	}
 out:
	return;
}

/**
 * base_proc_update - process new information received via ethercat.
 * update stats, reset EC_TIMEOUT
 *
 * @param base
 *
 * @return
 */
static int base_proc_update(struct youbot_base_info* base)
{
	int i, tmp, ret=-1;
	int32_t cm;
	in_motor_t *motor_in;
	out_motor_t *motor_out;
	int32_t cmd_cur_vel[YOUBOT_NR_OF_WHEELS];

	/* copy data */
	for(i=0; i<YOUBOT_NR_OF_WHEELS; i++) {
		motor_in = (in_motor_t*) ec_slave[ base->wheel_inf[i].slave_idx ].inputs;
		base->wheel_inf[i].msr_pos = motor_in->position;
		base->wheel_inf[i].msr_vel = motor_in->velocity;
		base->wheel_inf[i].msr_cur = motor_in->current;
		base->wheel_inf[i].error_flags = motor_in->error_flags;
#ifdef FIRMWARE_V2
		base->wheel_inf[i].msr_pwm = motor_in->pwm;
#endif
	}

	base_proc_errflg(base);


	/* Force control_mode to INITIALIZE if unconfigured */
	if(!base_is_configured(base)) {
		base->control_mode = YOUBOT_CMODE_INITIALIZE;
		goto handle_cm;
	} else if (base_is_configured(base) && base->control_mode == YOUBOT_CMODE_INITIALIZE) {
		DBG("init completed, switching to MOTORSTOP");
		base->control_mode = YOUBOT_CMODE_MOTORSTOP;
		tmp=YOUBOT_CMODE_MOTORSTOP;
		write_int(base->p_control_mode, &tmp);
	}

	/* new control mode? */
	if(read_int(base->p_control_mode, &cm) > 0) {
		if(cm<0 || cm>7) {
			ERR("invalid control_mode %d", cm);
			base->control_mode=YOUBOT_CMODE_MOTORSTOP;
			tmp=YOUBOT_CMODE_MOTORSTOP;
			write_int(base->p_control_mode, &tmp);
		} else {
			DBG("setting control_mode to %d", cm);
			base->control_mode=cm;
			tmp=cm;
			write_int(base->p_control_mode, &tmp);
		}
		/* reset output quantity upon control_mode switch */
		for(i=0; i<YOUBOT_NR_OF_WHEELS; i++)
			base->wheel_inf[i].cmd_val=0;

		base_check_watchdog(base, 1);
	}

	base_output_msr_data(base);

 handle_cm:

	switch(base->control_mode) {
	case YOUBOT_CMODE_MOTORSTOP:
		break;
	case YOUBOT_CMODE_VELOCITY:
		if(read_int4(base->p_cmd_vel, &cmd_cur_vel) == 4) { /* raw velocity */
			for(i=0; i<YOUBOT_NR_OF_WHEELS; i++)
				base->wheel_inf[i].cmd_val=cmd_cur_vel[i];
			base_check_watchdog(base, 1);
		} else if (read_kdl_twist(base->p_cmd_twist, &base->cmd_twist) > 0) { /* twist */
			twist_to_wheel_val(base);
			base_check_watchdog(base, 1);
		} else { /* no new data */
			/* if cmd_vel is zero, trigger watchdog */
			if(base->wheel_inf[0].cmd_val==0 && base->wheel_inf[1].cmd_val==0 &&
			   base->wheel_inf[2].cmd_val==0 && base->wheel_inf[3].cmd_val==0) {
				base_check_watchdog(base, 1);
			} else {
				base_check_watchdog(base, 0);
			}
		}
		break;
	case YOUBOT_CMODE_CURRENT:
		if(read_int4(base->p_cmd_cur, &cmd_cur_vel) == 4) { /* raw current */
			for(i=0; i<YOUBOT_NR_OF_WHEELS; i++)
				base->wheel_inf[i].cmd_val=cmd_cur_vel[i];
			base_check_watchdog(base, 1);
		} else {
			if(base->wheel_inf[0].cmd_val==0 && base->wheel_inf[1].cmd_val==0 &&
			   base->wheel_inf[2].cmd_val==0 && base->wheel_inf[3].cmd_val==0) {
				base_check_watchdog(base, 1);
			} else {
				base_check_watchdog(base, 0);
			}
		}
		break;
	case YOUBOT_CMODE_INITIALIZE:
		break;
	default:
		ERR("unexpected control_mode: %d, switching to MOTORSTOP", base->control_mode);
		base->control_mode = YOUBOT_CMODE_MOTORSTOP;
		break;
	}


	/* copy cmd_val to output buffer */
	for(i=0; i<YOUBOT_NR_OF_WHEELS; i++) {
		motor_out = (out_motor_t*) ec_slave[ base->wheel_inf[i].slave_idx ].outputs;
		motor_out->value = base->wheel_inf[i].cmd_val;
		motor_out->controller_mode = base->control_mode;
	}

	ret=0;
	return ret;
}


/*******************************************************************************
 *									       *
 *                            youbot arm driver code			       *
 *									       *
 *******************************************************************************/


/**
 * arm_proc_errflg - update statistics and return number of fatal
 * errors.
 *
 * @param ainf
 *
 * @return number of fatal errors
 */
static int arm_proc_errflg(struct youbot_arm_info* ainf)
{
	int fatal_errs=0, i, dummy;

	/* check and raise errors */
	for(i=0; i<YOUBOT_NR_OF_JOINTS; i++) {
		if(ainf->jnt_inf[i].error_flags & OVERCURRENT) {
			ainf->jnt_inf[i].stats.overcurrent++;
			ainf->control_mode=YOUBOT_CMODE_MOTORSTOP;
			fatal_errs++;
			DBG("OVERCURRENT on joint %i", i);
		}
		if(ainf->jnt_inf[i].error_flags & UNDERVOLTAGE) {
			ainf->jnt_inf[i].stats.undervoltage++;
			ainf->control_mode=YOUBOT_CMODE_MOTORSTOP;
			fatal_errs++;
			DBG("UNDERVOLTAGE on joint %i", i);
		}
		if(ainf->jnt_inf[i].error_flags & OVERTEMP) {
			ainf->jnt_inf[i].stats.overtemp++;
			ainf->control_mode=YOUBOT_CMODE_MOTORSTOP;
			fatal_errs++;
			DBG("OVERTEMP on joint %i", i);
		}
		if(ainf->jnt_inf[i].error_flags & HALL_ERR) {
			ainf->jnt_inf[i].stats.hall_err++;
			DBG("HALL_ERR on joint %i", i);
		}
		if(ainf->jnt_inf[i].error_flags & ENCODER_ERR) {
			ainf->jnt_inf[i].stats.encoder_err++;
			DBG("ENCODER_ERR on joint %i", i);
		}
		if(ainf->jnt_inf[i].error_flags & SINE_COMM_INIT_ERR) {
			ainf->jnt_inf[i].stats.sine_comm_init_err++;
			DBG("SINE_COMM_INIT_ERR on joint %i", i);
		}
		if(ainf->jnt_inf[i].error_flags & EMERGENCY_STOP) {
			ainf->jnt_inf[i].stats.emergency_stop++;
			DBG("EMERGENCY_STOP on joint %i", i);
		}
		if(ainf->jnt_inf[i].error_flags & MODULE_INIT) {
			/* if (! (ainf->mod_init_stat & (1 << i))) DBG("MODULE_INIT set for joint %d", i); */
			ainf->mod_init_stat |= 1<<i;
		}
		if(ainf->jnt_inf[i].error_flags & EC_TIMEOUT) {
			ainf->jnt_inf[i].stats.ec_timeout++;
			DBG("EC_TIMEOUT on joint %i", i);
			if(send_mbx(0, SAP, CLR_EC_TIMEOUT, ainf->jnt_inf[i].slave_idx, 0, &dummy)) {
				ERR("failed to clear EC_TIMEOUT flag");
				fatal_errs++;
			}
		}

		if(ainf->jnt_inf[i].error_flags & I2T_EXCEEDED && !ainf->jnt_inf[i].i2t_ex) {
			/* i2t error rising edge */
			ERR("I2T_EXCEEDED on joint %i setting arm to control_mode=MotorStop.", i);
			ainf->jnt_inf[i].stats.i2t_exceeded++;
			ainf->jnt_inf[i].i2t_ex=1;
			ainf->control_mode=YOUBOT_CMODE_MOTORSTOP;
			fatal_errs++;

			if(send_mbx(0, SAP, CLEAR_I2T, ainf->jnt_inf[i].slave_idx, 0, &dummy))
				ERR("joint %d, failed to clear I2T_EXCEEDED bit", i);

		} else if (!(ainf->jnt_inf[i].error_flags & I2T_EXCEEDED) && ainf->jnt_inf[i].i2t_ex) {
			/* i2t error falling edge */
			DBG("I2T_EXCEEDED reset on joint %i", i);
			ainf->jnt_inf[i].i2t_ex=0;
		}
	}

	return fatal_errs;
}


/**
 * Scale down the cmd output according to limits.
 *
 * More specifically, scale down proportionally when inside the
 * soft-limits and moving _towards_ the hard-limit.
 *
 * @param arm arm_info struct.
 * @param jnt_num (0-4) joint to scale down.
 */
static int32_t scale_down_cmd_val(struct youbot_arm_info* arm, int jnt_num)
{
	double jntpos, upper_start, upper_end, lower_start, lower_end, scale_val;
	int32_t cmd_val;

	cmd_val = arm->jnt_inf[jnt_num].cmd_val;

	if(arm->jntlim_safety_disabled || arm->control_mode == YOUBOT_CMODE_POSITIONING)
		goto out;

#ifdef USE_ARM_JNT_STATE_STRUCT
	jntpos = arm->jnt_states.pos[jnt_num];
#else
	jntpos = arm->pos[jnt_num];
#endif
	upper_start = youbot_arm_upper_limits[jnt_num] * YOUBOT_ARM_SCALE_START * (M_PI/180);
	upper_end = youbot_arm_upper_limits[jnt_num] * YOUBOT_ARM_SCALE_END * (M_PI/180);
	lower_start = youbot_arm_lower_limits[jnt_num] * YOUBOT_ARM_SCALE_START * (M_PI/180);
	lower_end = youbot_arm_lower_limits[jnt_num] * YOUBOT_ARM_SCALE_END * (M_PI/180);

	scale_val=1;

	/* outside of range */
	if (jntpos < upper_start && jntpos > lower_start)
		goto out;

	/* inside upper range and moving towards limit. */
	if (jntpos > upper_start && cmd_val > 0) {
		scale_val = (upper_end - jntpos)/(upper_end - upper_start);
		goto scale;
	}

	/* inside lower range and moving towards limit */
	if (jntpos < lower_start && cmd_val < 0) {
		scale_val = (lower_end - jntpos)/(lower_end - lower_start);
		goto scale;
	}
	/* should never get here */
	goto out;
 scale:
	cmd_val*=scale_val;
 out:
	return cmd_val;
}


static int control_gripper(struct youbot_arm_info* arm, int32_t pos1, int32_t pos2)
{
	if(send_mbx(1, MVP, 1, 11, 0, &pos1))
		ERR("failed to move finger 1 to pos %d", pos1);
	if(send_mbx(1, MVP, 1, 11, 1, &pos2))
		ERR("failed to move finger 2 to pos %d", pos2);
	return 0;
}

static int arm_is_configured(struct youbot_arm_info* arm)
{
	return (arm->mod_init_stat == ((1 << YOUBOT_NR_OF_JOINTS)-1));
}

static void arm_output_msr_data(struct youbot_arm_info* arm)
{
	int i;
	struct youbot_arm_motorinfo motorinf;
	/* struct youbot_arm_state arm_state; */

	/* copy and write out the "raw" driver information */
	for(i=0; i<YOUBOT_NR_OF_JOINTS; i++) {
		motorinf.pos[i]=arm->jnt_inf[i].msr_pos;
		motorinf.vel[i]=arm->jnt_inf[i].msr_vel;
		motorinf.cur[i]=arm->jnt_inf[i].msr_cur;

#ifdef USE_ARM_JNT_STATE_STRUCT
		arm->jnt_states.pos[i] = ARM_TICKS_TO_POS * arm->jnt_inf[i].msr_pos;
		arm->jnt_states.vel[i] = ARM_RPM_TO_VEL * arm->jnt_inf[i].msr_vel;
		arm->jnt_states.eff[i] = ARM_CUR_TO_EFF * arm->jnt_inf[i].msr_cur;
#else
		arm->pos[i] = ARM_TICKS_TO_POS * arm->jnt_inf[i].msr_pos;
		arm->vel[i] = ARM_RPM_TO_VEL * arm->jnt_inf[i].msr_vel;
		arm->eff[i] = ARM_CUR_TO_EFF * arm->jnt_inf[i].msr_cur;
#endif
	}

	write_arm_motorinfo(arm->p_arm_motorinfo, &motorinf);
#ifdef USE_ARM_JNT_STATE_STRUCT
	write_arm_state(arm->p_arm_state, &arm->jnt_states);
#else
	write_double5(arm->p_msr_pos, &arm->pos);
	write_double5(arm->p_msr_vel, &arm->vel);
	write_double5(arm->p_msr_eff, &arm->eff);
#endif
}

#if 0
static int32_t arm_is_calibrated()
{
	int32_t res;
	/* grip=0, GGP, param_nr=16, slave=1, bank=2 (user) */
	if(send_mbx(0, GGP, 16, 1, 2, &res))
		ERR("failed to read volatile calibrated state");
	return res==3001;
}

static void arm_set_calibrated()
{
	int32_t val = 3001;
	if(send_mbx(0, SGP, 16, 1, 2, &val))
		ERR("failed to set volatile calibrated state");
}
#endif

/**
 * Move all arm joints into limits
 *
 * @param arm pointer to struct youbot_arm_info
 *
 * @return 1 when completed, zero otherwise.
 */
static int arm_move_to_lim(struct youbot_arm_info* arm)
{
	int i, ret=1;

	arm->control_mode=YOUBOT_CMODE_VELOCITY;

	for(i=0; i<YOUBOT_NR_OF_JOINTS; i++) {
		if(arm->jnt_inf[i].msr_cur > arm_calib_cur_high[i]) { /* jnt completed? */
			/* DBG("calibration: axis %d in limit", i); */
			arm->jnt_inf[i].cmd_val = 0;
			arm->axis_at_limit[i]=1;
		} else if(arm->axis_at_limit[i] != 1) { /* not finished yet */
			arm->jnt_inf[i].cmd_val = arm_calib_move_in_vel[i] / ARM_RPM_TO_VEL;
		}
		ret = ret && arm->axis_at_limit[i];
	}

	/* are we in limits? */
	if(ret==1) {
		for(i=0; i<YOUBOT_NR_OF_JOINTS; i++) {
			arm->axis_at_limit[i]=0;
		}
	}

	return ret;
}

static int arm_proc_update(struct youbot_arm_info* arm)
{
	int i, tmp, ret=-1;
	int32_t cm, gripper_cmd;
	in_motor_t *motor_in;
	out_motor_t *motor_out;
	double cmd_pos[YOUBOT_NR_OF_JOINTS];
	double cmd_vel[YOUBOT_NR_OF_JOINTS];
	double cmd_eff[YOUBOT_NR_OF_JOINTS];
	int32_t cmd_cur[YOUBOT_NR_OF_JOINTS];

	/* copy data */
	for(i=0; i<YOUBOT_NR_OF_JOINTS; i++) {
		motor_in = (in_motor_t*) ec_slave[ arm->jnt_inf[i].slave_idx ].inputs;
		arm->jnt_inf[i].msr_pos = motor_in->position;
		arm->jnt_inf[i].msr_vel = motor_in->velocity;
		arm->jnt_inf[i].msr_cur = motor_in->current;
		arm->jnt_inf[i].error_flags = motor_in->error_flags;
#ifdef FIRMWARE_V2
		arm->jnt_inf[i].msr_pwm = motor_in->pwm;
#endif
	}

	arm_proc_errflg(arm);

	/* Force control_mode to INITIALIZE if unconfigured */
	if(!arm_is_configured(arm)) {
		arm->control_mode = YOUBOT_CMODE_INITIALIZE;
		goto handle_cm;
	} else if (arm_is_configured(arm) && arm->control_mode == YOUBOT_CMODE_INITIALIZE) {
		DBG("init completed, switching to MOTORSTOP");
		arm->control_mode = YOUBOT_CMODE_MOTORSTOP;
		tmp=YOUBOT_CMODE_MOTORSTOP;
		write_int(arm->p_control_mode, &tmp);
	}

	arm_output_msr_data(arm);

	/* calibration requested? */
	if(read_int(arm->p_calibrate_cmd, &tmp) > 0) {
		DBG("calibration: starting");
		arm->calibrating=1;
		arm->jntlim_safety_disabled=1;
	}

	/* calibration "FSM" */
	if(arm->calibrating) {
		/* moving to limits */
		if(arm->calibrating == 1) {
			if(arm_move_to_lim(arm)==1) {
				arm->calibrating=2;
				arm->control_mode=YOUBOT_CMODE_SET_POSITION_TO_REFERENCE;
			}
		} else if(arm->calibrating == 2) {
			/* move to home? */
			arm->calibrating=0;
			arm->jntlim_safety_disabled=0;
			arm->control_mode=YOUBOT_CMODE_MOTORSTOP;
			DBG("calibration: completed!");
#if 0
			arm_set_calibrated();
#endif
		}

		/* when calibrating, control mode switches, etc are
		 * not allowed */
		goto handle_cm;
	}

	/* new control mode? */
	if(read_int(arm->p_control_mode, &cm) > 0) {
		if(cm<0 || cm>7) {
			ERR("invalid control_mode %d", cm);
			arm->control_mode=YOUBOT_CMODE_MOTORSTOP;
			tmp=YOUBOT_CMODE_MOTORSTOP;
		} else {
			DBG("setting control_mode to %d", cm);
			arm->control_mode=cm;
			tmp=cm;
		}

		write_int(arm->p_control_mode, &tmp);

		/* reset output quantity upon control_mode switch */
		for(i=0; i<YOUBOT_NR_OF_JOINTS; i++)
			arm->jnt_inf[i].cmd_val=0;
	}

	/* gripper */
	if(read_int(arm->p_gripper, &gripper_cmd) > 0) {
		gripper_cmd = (gripper_cmd > 0) ? -GRIPPER_ENC_WIDTH : GRIPPER_ENC_WIDTH;
		control_gripper(arm, gripper_cmd, gripper_cmd);
	}

 handle_cm:
	switch(arm->control_mode) {
	case YOUBOT_CMODE_MOTORSTOP:
		break;
	case YOUBOT_CMODE_POSITIONING:
		if(read_double5(arm->p_cmd_pos, &cmd_pos) == YOUBOT_NR_OF_JOINTS) { /* raw position */
			for(i=0; i<YOUBOT_NR_OF_JOINTS; i++)
				arm->jnt_inf[i].cmd_val = cmd_pos[i] / ARM_TICKS_TO_POS;
		}
		break;
	case YOUBOT_CMODE_VELOCITY:  /* raw velocity */
		if (arm->calibrating==0 && read_double5(arm->p_cmd_vel, &cmd_vel) == YOUBOT_NR_OF_JOINTS) {
			for(i=0; i<YOUBOT_NR_OF_JOINTS; i++)
				arm->jnt_inf[i].cmd_val = cmd_vel[i] / ARM_RPM_TO_VEL;
		}
		break;
	case YOUBOT_CMODE_CURRENT:
		if(read_double5(arm->p_cmd_eff, &cmd_eff) == YOUBOT_NR_OF_JOINTS) { /* efforts */
			for(i=0; i<YOUBOT_NR_OF_JOINTS; i++)
				arm->jnt_inf[i].cmd_val = cmd_eff[i] / ARM_CUR_TO_EFF;
		} else if(read_int5(arm->p_cmd_cur, &cmd_cur) == YOUBOT_NR_OF_JOINTS) { /* raw current */
			for(i=0; i<YOUBOT_NR_OF_JOINTS; i++)
				arm->jnt_inf[i].cmd_val = cmd_cur[i];
		}
		break;
	case YOUBOT_CMODE_INITIALIZE:
		break;
	case YOUBOT_CMODE_SET_POSITION_TO_REFERENCE:
		for(i=0; i<YOUBOT_NR_OF_JOINTS; i++) {
			arm->jnt_inf[i].cmd_val = arm_calib_ref_pos[i] / ARM_TICKS_TO_POS;
		}
		break;
	default:
		ERR("unexpected control_mode: %d, switching to MOTORSTOP", arm->control_mode);
		arm->control_mode = YOUBOT_CMODE_MOTORSTOP;
		break;
	}

	/* copy cmd_val to output buffer */
	for(i=0; i<YOUBOT_NR_OF_JOINTS; i++) {
		motor_out = (out_motor_t*) ec_slave[ arm->jnt_inf[i].slave_idx ].outputs;
		motor_out->value = scale_down_cmd_val(arm, i); /* active if jntlim_safety_disabled==0; */
		motor_out->controller_mode = arm->control_mode;
	}

	ret=0;
	return ret;
}

/*******************************************************************************
 *									       *
 *                            Microblx block code			       *
 *									       *
 *******************************************************************************/

static int youbot_init(ubx_block_t *b)
{
	int ret=-1, i, cur_slave;
	unsigned int len;
	char* interface;
	uint32_t *conf_nr_arms;
	struct youbot_info *inf;

	interface = (char *) ubx_config_get_data_ptr(b, "ethernet_if", &len);
	conf_nr_arms = (uint32_t *) ubx_config_get_data_ptr(b, "nr_arms", &len);

	if(strncmp(interface, "", len)==0) {
		ERR("config ethernet_if unset");
		goto out;
	}

	/* allocate driver data */
	if((inf=calloc(1, sizeof(struct youbot_info)))==NULL) {
		ERR("failed to alloc youbot_info");
		goto out;
	}

	b->private_data=inf;

	if(ec_init(interface) <= 0) {
		ERR("ec_init failed (IF=%s), sufficient rights, correct interface?", interface);
		goto out_free;
	}

	if(ec_config(TRUE, &inf->io_map) <= 0) {
		ERR("ec_config failed  (IF=%s)", interface);
		goto out_free;
	} else {
		DBG("detected %d slaves", ec_slavecount);
	}

	/* Let's see what we have...
	 * check for base, base is mandatory for a youbot */
	if(ec_slavecount >= YOUBOT_NR_OF_BASE_SLAVES && validate_base_slaves(1) == 0) {
		DBG("detected youbot base");
		inf->base.detected=1;

		for(i=0, cur_slave=2; i<YOUBOT_NR_OF_WHEELS; i++, cur_slave++)
			inf->base.wheel_inf[i].slave_idx=cur_slave;

	} else {
		ERR("no base detected");
		goto out_free;
	}

	/* check for first arm */
	if(ec_slavecount >= (YOUBOT_NR_OF_BASE_SLAVES + YOUBOT_NR_OF_ARM_SLAVES) &&
	   validate_arm_slaves(1+YOUBOT_NR_OF_BASE_SLAVES)==0) {
		DBG("detected youbot arm #1 (first slave=%d)", cur_slave);
		inf->arm1.detected=1;
		*conf_nr_arms=1;
		cur_slave++; /* skip the power board */

		for(i=0; i<YOUBOT_NR_OF_JOINTS; i++, cur_slave++)
			inf->arm1.jnt_inf[i].slave_idx=cur_slave;
	}

	/* check for second arm */
	if(ec_slavecount >= (YOUBOT_NR_OF_BASE_SLAVES + (2 * YOUBOT_NR_OF_ARM_SLAVES)) &&
	   validate_arm_slaves(1+YOUBOT_NR_OF_BASE_SLAVES+YOUBOT_NR_OF_ARM_SLAVES)==0) {
		DBG("detected youbot arm #2 (first slave=%d)", cur_slave);
		inf->arm2.detected=1;
		*conf_nr_arms=2;
		cur_slave++; /* skip the power board */

		for(i=0; i<YOUBOT_NR_OF_JOINTS; i++, cur_slave++)
			inf->arm2.jnt_inf[i].slave_idx=cur_slave;
	}

	/* wait for all slaves to reach SAFE_OP state */
	ec_statecheck(0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE);

	if(ec_slave[0].state != EC_STATE_SAFE_OP) {
		ERR("not all slaves reached state safe_operational:");
		for (i=0; i<=ec_slavecount; i++) {
			if (ec_slave[i].state != EC_STATE_SAFE_OP) {
				ERR("\tslave %d (%s) is in state=0x%x, ALstatuscode=0x%x",
				    i, ec_slave[i].name, ec_slave[i].state, ec_slave[i].ALstatuscode);
			}
		}
		goto out_free;
	}

	/* send and receive bogus process data to get things going */
	if(ec_send_processdata() <= 0) {
		ERR("failed to send bootstrap process data");
		goto out_free;
	}

	if(ec_receive_processdata(EC_TIMEOUTRET) == 0) {
		ERR("failed to receive bootstrap process data");
		goto out_free;
	}

	if(inf->base.detected) base_config_params(&inf->base);
	if(inf->arm1.detected) arm_config_params(&inf->arm1);
	if(inf->arm2.detected) arm_config_params(&inf->arm2);

#if 0
	int tmp
	/* find firmware version */
	if(send_mbx(0, FIRMWARE_VERSION, 1, 0, 0, &tmp)) {
		ERR("Failed to read firmware version");
		goto out;
	}
	else
		DBG("youbot firmware version %d", tmp);

#endif
	ret=0;
	goto out;

 out_free:
	free(b->private_data);
 out:
	return ret;
}


static int youbot_start(ubx_block_t *b)
{
	DBG(" ");
	int ret=-1, i;
	struct youbot_info *inf=b->private_data;

	/* reset old commands */
	if(inf->base.detected && base_prepare_start(&inf->base) != 0) goto out;
	if(inf->arm1.detected && arm_prepare_start(&inf->arm1) != 0) goto out;
	if(inf->arm2.detected && arm_prepare_start(&inf->arm2) != 0) goto out;

	/* requesting operational for all slaves */
	ec_slave[0].state = EC_STATE_OPERATIONAL;
	ec_writestate(0);

	/* wait for all slaves to reach SAFE_OP state */
	ec_statecheck(0, EC_STATE_OPERATIONAL, EC_TIMEOUTSTATE);

	if(ec_slave[0].state != EC_STATE_OPERATIONAL) {
		ERR("not all slaves reached state operational:");
		for (i=0; i<=ec_slavecount; i++) {
			if (ec_slave[i].state != EC_STATE_OPERATIONAL) {
				ERR("\tslave %d (%s) is in state=0x%x, ALstatuscode=0x%x",
				    i, ec_slave[i].name, ec_slave[i].state, ec_slave[i].ALstatuscode);
			}
		}
		goto out;
	}

	/* get things rolling */
	if (ec_send_processdata() <= 0) {
		inf->pd_send_err++;
		ERR("sending initial process data failed");
	}

	/* cache port pointers */
	assert(inf->base.p_control_mode = ubx_port_get(b, "base_control_mode"));
	assert(inf->base.p_cmd_twist = ubx_port_get(b, "base_cmd_twist"));
	assert(inf->base.p_cmd_vel = ubx_port_get(b, "base_cmd_vel"));
	assert(inf->base.p_cmd_cur = ubx_port_get(b, "base_cmd_cur"));
	assert(inf->base.p_msr_odom = ubx_port_get(b, "base_msr_odom"));
	assert(inf->base.p_msr_twist = ubx_port_get(b, "base_msr_twist"));
	assert(inf->base.p_base_motorinfo = ubx_port_get(b, "base_motorinfo"));

	assert(inf->arm1.p_control_mode = ubx_port_get(b, "arm1_control_mode"));
	assert(inf->arm1.p_calibrate_cmd = ubx_port_get(b, "arm1_calibrate_cmd"));
	assert(inf->arm1.p_cmd_pos = ubx_port_get(b, "arm1_cmd_pos"));
	assert(inf->arm1.p_cmd_vel = ubx_port_get(b, "arm1_cmd_vel"));
	assert(inf->arm1.p_cmd_cur = ubx_port_get(b, "arm1_cmd_cur"));
	assert(inf->arm1.p_cmd_eff = ubx_port_get(b, "arm1_cmd_eff"));
#ifdef USE_ARM_JNT_STATE_STRUCT
	assert(inf->arm1.p_arm_state = ubx_port_get(b, "arm1_state"));
#else
	assert(inf->arm1.p_msr_pos = ubx_port_get(b, "arm1_msr_pos"));
	assert(inf->arm1.p_msr_vel = ubx_port_get(b, "arm1_msr_vel"));
	assert(inf->arm1.p_msr_eff = ubx_port_get(b, "arm1_msr_eff"));
#endif
	assert(inf->arm1.p_arm_motorinfo = ubx_port_get(b, "arm1_motorinfo"));
	assert(inf->arm1.p_gripper = ubx_port_get(b, "arm1_gripper"));

	ret=0;
 out:
	return ret;
}

static void youbot_stop(ubx_block_t *b)
{
	DBG(" ");
}


static void youbot_step(ubx_block_t *b)
{
	struct youbot_info *inf=b->private_data;

	/* TODO set time out to zero? */
	if(ec_receive_processdata(EC_TIMEOUTRET) == 0) {
		ERR("failed to receive processdata");
		inf->pd_recv_err++;

		goto out_send;
	}

	if(inf->base.detected) base_proc_update(&inf->base);
	if(inf->arm1.detected) arm_proc_update(&inf->arm1);
	if(inf->arm2.detected) arm_proc_update(&inf->arm2);

out_send:

	if (ec_send_processdata() <= 0){
		ERR("failed to send processdata");
		inf->pd_send_err++;
	}

	return;
}

static void youbot_cleanup(ubx_block_t *b)
{
	ec_slave[0].state = EC_STATE_PRE_OP;
	ec_writestate(0);
	ec_close();
	free((struct youbot_info*) b->private_data);
}


/* put everything together */
ubx_block_t youbot_comp = {
	.name = "youbot/youbot_driver",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = youbot_meta,
	.configs = youbot_config,
	.ports = youbot_ports,

	/* ops */
	.init = youbot_init,
	.start = youbot_start,
	.stop = youbot_stop,
	.step = youbot_step,
	.cleanup = youbot_cleanup,
};

/* module init and cleanup */
static int youbot_mod_init(ubx_node_info_t* ni)
{
	ubx_type_t *tptr;
	for(tptr=youbot_types; tptr->name!=NULL; tptr++)
		ubx_type_register(ni, tptr);

	return ubx_block_register(ni, &youbot_comp);
}

static void youbot_mod_cleanup(ubx_node_info_t *ni)
{
	const ubx_type_t *tptr;

	for(tptr=youbot_types; tptr->name!=NULL; tptr++)
		ubx_type_unregister(ni, tptr->name);

	ubx_block_unregister(ni, "youbot/youbot_driver");
}

UBX_MODULE_INIT(youbot_mod_init)
UBX_MODULE_CLEANUP(youbot_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause LGPL-2.1+)
