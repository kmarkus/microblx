/*
 * A youbot driver block.
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
	"{ doc='A youbot driver block',"
	"  license='LGPL',"
	"  real-time=true,"
	"  aperiodic=true, "
	"}";


ubx_config_t youbot_config[] = {
	{ .name="ethernet_if", .type_name = "char" },
	{ NULL },
};

ubx_port_t youbot_ports[] = {
	{ .name="events", .attrs=PORT_DIR_OUT, .in_type_name="unsigned int" },

	{ .name="base_control_mode", .attrs=PORT_DIR_IN, .in_type_name="int32_t", .out_type_name="int32_t" },
	{ .name="base_cmd_twist", .attrs=PORT_DIR_IN, .in_type_name="kdl/struct kdl_twist" },
	{ .name="base_cmd_vel", .attrs=PORT_DIR_IN, .in_type_name="int32_t", .in_data_len=4 },
	{ .name="base_cmd_cur", .attrs=PORT_DIR_IN, .in_type_name="int32_t", .in_data_len=4 },

	{ .name="base_msr_odom", .attrs=PORT_DIR_OUT, .out_type_name="youbot/struct youbot_odometry" },
	{ .name="base_msr_twist", .attrs=PORT_DIR_OUT, .out_type_name="kdl/struct kdl_twist" },

	{ NULL },
};

/* types */
/* declare types */

#include "types/youbot_odometry.h.hexarr"
#include "types/youbot_control_modes.h.hexarr"

ubx_type_t youbot_types[] = {
	def_struct_type("youbot", struct youbot_odometry, &youbot_odometry_h),
	/* def_struct_type("youbot", enum youbot_control_modes, &youbot_control_modes_h), */
	{ NULL },
};


/* convenience functions to read/write from the ports */
def_read_fun(read_int, int32_t)
def_write_fun(write_int, int32_t)

def_read_fun(read_kdl_twist, struct kdl_twist)
def_write_fun(write_kdl_twist, struct kdl_twist)


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
 * @param bank_nr always zero. Motor or Bank number
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

	DBG("sending send mbx (gripper:%d, instr_nr=%d, param_nr=%d, slave_nr=%d, bank_nr=%d, value=%d",
	    gripper, instr_nr, param_nr, slave_nr, bank_nr, *value);

	if (ec_mbxsend(slave_nr, &mbx_out, EC_TIMEOUTSAFE) <= 0) {
		ERR("failed to send mbx (gripper:%d, instr_nr=%d, param_nr=%d, slave_nr=%d, bank_nr=%d, value=%d",
		    gripper, instr_nr, param_nr, slave_nr, bank_nr, *value);
		goto out;
	}

	if (ec_mbxreceive(slave_nr, &mbx_in, EC_TIMEOUTSAFE) <= 0) {
		ERR("failed to receive mbx (gripper:%d, instr_nr=%d, param_nr=%d, slave_nr=%d, bank_nr=%d, value=%d",
		    gripper, instr_nr, param_nr, slave_nr, bank_nr, *value);
		goto out;
	}

	*value = (mbx_in[4] << 24 | mbx_in[5] << 16 | mbx_in[6] << 8 | mbx_in[7]);

	if(((int) mbx_in[2]) != 100) {
		ERR("receiving mbx failed: module=%d, status=%d, command=%d, value=%d",
		    mbx_in[1], mbx_in[2], mbx_in[3], *value);
		goto out;
	}

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

static int base_config_params(struct youbot_base_info* binf)
{
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

	/* read max_current into local data */
	for(i=0; i<YOUBOT_NR_OF_JOINTS; i++) {
		if(send_mbx(0, GAP, MAX_CURRENT, ainf->jnt_inf[i].slave_idx, 0, &val)) {
			ERR("reading MAX_CURRENT failed");
			goto out;
		}
		ainf->jnt_inf[i].max_cur = val;
		DBG("limiting axis[%d] to max_current %d", i, val);
	}
 out:
	return ret;
}


/*
 * Microblx driver wrapper starts here.
 */


static int youbot_init(ubx_block_t *b)
{
	int ret=-1, i, cur_slave;
	unsigned int interface_len;
	char* interface;
	struct youbot_info *inf;

	interface = (char *) ubx_config_get_data_ptr(b, "ethernet_if", &interface_len);

	if(strncmp(interface, "", interface_len)==0) {
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
	   validate_arm_slaves(1+YOUBOT_NR_OF_BASE_SLAVES)) {
		DBG("detected first youbot arm");
		inf->arm1.detected=1;

		for(i=0, cur_slave=YOUBOT_NR_OF_BASE_SLAVES+1; i<YOUBOT_NR_OF_JOINTS; i++, cur_slave++)
			inf->arm1.jnt_inf[i].slave_idx=cur_slave;
	}

	/* check for second arm */
	if(ec_slavecount >= (YOUBOT_NR_OF_BASE_SLAVES + (2 * YOUBOT_NR_OF_ARM_SLAVES)) &&
	   validate_arm_slaves(1+YOUBOT_NR_OF_BASE_SLAVES+YOUBOT_NR_OF_ARM_SLAVES)) {
		DBG("detected second youbot arm");
		inf->arm2.detected=1;

		for(i=0, cur_slave=(YOUBOT_NR_OF_BASE_SLAVES+YOUBOT_NR_OF_ARM_SLAVES+1);
		    i<YOUBOT_NR_OF_JOINTS; i++, cur_slave++)
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

	ret=0;
	goto out;

 out_free:
	free(b->private_data);
 out:
	return ret;
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

/* set all commanded quantities to zero */
static int arm_prepare_start(struct youbot_arm_info *arm)
{
	int i, ret=-1;
	int32_t dummy;

	for(i=0; i<YOUBOT_NR_OF_JOINTS; i++)
		arm->jnt_inf[i].cmd_val=0;

	/* reset EC_TIMEOUT */
	for(i=0; i<YOUBOT_NR_OF_JOINTS; i++) {
		if(send_mbx(0, SAP, CLR_EC_TIMEOUT, arm->jnt_inf[i].slave_idx, 0, &dummy)) {
			ERR("failed to clear EC_TIMEOUT");
			goto out;
		}
	}

	ret=0;
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
	inf->base.p_control_mode = ubx_port_get(b, "base_control_mode");
	inf->base.p_cmd_twist = ubx_port_get(b, "base_cmd_twist");
	inf->base.p_cmd_vel = ubx_port_get(b, "base_cmd_vel");
	inf->base.p_cmd_cur = ubx_port_get(b, "base_cmd_cur");
	inf->base.p_msr_odom = ubx_port_get(b, "base_odom");
	inf->base.p_msr_twist = ubx_port_get(b, "base_twist");

	ret=0;
 out:
	return ret;
}

static void youbot_stop(ubx_block_t *b)
{
	DBG(" ");
}

/**
 * base_proc_errflg - update statistics and return number of fatal
 * errors.
 *
 * @param minf
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
			ERR("OVERCURRENT on wheel %i", i);
		}
		if(binf->wheel_inf[i].error_flags & UNDERVOLTAGE) {
			binf->wheel_inf[i].stats.undervoltage++; fatal_errs++;
			ERR("UNDERVOLTAGE on wheel %i", i);
		}
		if(binf->wheel_inf[i].error_flags & OVERTEMP) {
			binf->wheel_inf[i].stats.overtemp++; fatal_errs++;
			ERR("OVERTEMP on wheel %i", i);
		}
		if(binf->wheel_inf[i].error_flags & HALL_ERR) {
			binf->wheel_inf[i].stats.hall_err++;
			ERR("HALL_ERR on wheel %i", i);
		}
		if(binf->wheel_inf[i].error_flags & ENCODER_ERR) {
			binf->wheel_inf[i].stats.encoder_err++;
			ERR("ENCODER_ERR on wheel %i", i);
		}
		if(binf->wheel_inf[i].error_flags & SINE_COMM_INIT_ERR) {
			binf->wheel_inf[i].stats.sine_comm_init_err++;
			ERR("SINE_COMM_INIT_ERR on wheel %i", i);
		}
		if(binf->wheel_inf[i].error_flags & EMERGENCY_STOP) {
			binf->wheel_inf[i].stats.emergency_stop++;
			ERR("EMERGENCY_STOP on wheel %i", i);
		}
		if(binf->wheel_inf[i].error_flags & MODULE_INIT) {
			if (! (binf->mod_init_stat & (1 << i)))
				DBG("MODULE_INIT set for wheel %d", i);
			binf->mod_init_stat |= 1<<i;
		}
		if(binf->wheel_inf[i].error_flags & EC_TIMEOUT) {
			binf->wheel_inf[i].stats.ec_timeout++;
			ERR("EC_TIMEOUT on wheel %i", i);
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

	/* TODO: the previous odometry implementation is not working
	   correctly. Find out why before adding */
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
	return base->mod_init_stat & ((1 << YOUBOT_NR_OF_BASE_SLAVES)-1);
}

/**
 * check_watchdog - check if expired and trigger.
 * currently only second resolution!
 *
 * @param base
 * @param trig if != 0 then trigger watchdog
 */
static void check_watchdog(struct youbot_base_info* base, int trig)
{
	struct timespec ts_cur;

	if(clock_gettime(CLOCK_MONOTONIC, &ts_cur) != 0) {
		ERR2(errno, "clock_gettime failed");
		goto out;
	}

	if(ts_cur.tv_sec - base->last_cmd.tv_sec >= BASE_TIMEOUT) {
		base->control_mode=YOUBOT_CMODE_MOTORSTOP;
		goto out;
	}

	if(trig)
		base->last_cmd = ts_cur;

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
	}

	/* new control mode? */
	if(read_int(base->p_control_mode, &cm) > 0) {
		if(cm<0 || cm>7) {
			ERR("invalid control_mode %d", cm);
			tmp=-1;
			write_int(base->p_control_mode, &tmp);
		} else {
			DBG("setting control_mode to %d", cm);
			base->control_mode=cm;
			tmp=0;
			write_int(base->p_control_mode, &tmp);
		}
	}

	base_output_msr_data(base);

 handle_cm:

	switch(base->control_mode) {
	case YOUBOT_CMODE_MOTORSTOP:
		break;
	case YOUBOT_CMODE_VELOCITY:

		if(read_kdl_twist(base->p_cmd_twist, &base->cmd_twist) <= 0) {
			/* no new data */
			if(base->wheel_inf[0].cmd_val==0 && base->wheel_inf[1].cmd_val==0 &&
			   base->wheel_inf[2].cmd_val==0 && base->wheel_inf[3].cmd_val==0) {
				check_watchdog(base, 1);
			} else {
				check_watchdog(base, 0);
			}
			break;
		}

		/* update wheel[i].cmd_val, values are copied to output below */
		check_watchdog(base, 1);
		twist_to_wheel_val(base);

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


static void youbot_step(ubx_block_t *b)
{
	DBG(" ");
	struct youbot_info *inf=b->private_data;

	/* TODO set time out to zero? */
	if(ec_receive_processdata(EC_TIMEOUTRET) == 0) {
		ERR("failed to receive processdata");
		inf->pd_recv_err++;
		goto out;
	}

	/* process update */
	if(inf->base.detected)
		base_proc_update(&inf->base);

	if (ec_send_processdata() <= 0){
		ERR("failed to send processdata");
		inf->pd_send_err++;
	}

 out:
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

static int youbot_mod_init(ubx_node_info_t* ni)
{
	DBG(" ");
	ubx_type_t *tptr;
	for(tptr=youbot_types; tptr->name!=NULL; tptr++)
		ubx_type_register(ni, tptr);

	return ubx_block_register(ni, &youbot_comp);
}

static void youbot_mod_cleanup(ubx_node_info_t *ni)
{
	DBG(" ");
	const ubx_type_t *tptr;

	for(tptr=youbot_types; tptr->name!=NULL; tptr++)
		ubx_type_unregister(ni, tptr->name);

	ubx_block_unregister(ni, "youbot/youbot_driver");
}

module_init(youbot_mod_init)
module_cleanup(youbot_mod_cleanup)
