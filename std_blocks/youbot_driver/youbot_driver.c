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

#include "ubx.h"

#include "youbot_driver.h"

/* communicated types */
#include "types/control_modes.h"


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

	{ .name="base_control_mode", .attrs=PORT_DIR_IN, .out_type_name="int32_t" },
	{ .name="base_odometry", .attrs=PORT_DIR_OUT, .in_type_name="int32_t" },
	{ .name="base_cmd_twist", .attrs=PORT_DIR_IN, .out_type_name="int32_t" },
	{ .name="base_cmd_current", .attrs=PORT_DIR_IN, .out_type_name="int32_t" },

	{ NULL },
};

/* convenience functions to read/write from the ports */
def_read_fun(read_int, int32_t)
def_write_fun(write_int, int32_t)


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
		if(strcmp(ec_slave[i].name, YOUBOT_WHEEL_CTRL_NAME) != 0 ||
		   strcmp(ec_slave[i].name, YOUBOT_WHEEL_CTRL_NAME2) != 0) {
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
		if(strcmp(ec_slave[i].name, YOUBOT_ARM_CTRL_NAME) != 0 ||
		   strcmp(ec_slave[i].name, YOUBOT_ARM_CTRL_NAME2) != 0) {
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
	     uint8_t slave_nr,
	     uint8_t bank_nr,
	     int32_t *value)
{
	int ret=-1;
	ec_mbxbuft mbx_out, mbx_in;

	mbx_out[0] = (gripper==0) ? 1 : 0;
	mbx_out[1] = instr_nr;
	mbx_out[2] = param_nr;
	mbx_out[3] = bank_nr;

	/* TODO: replace with htobe32 */
	mbx_out[4] = (uint32)*value >> 24;
	mbx_out[5] = (uint32)*value >> 16;
	mbx_out[6] = (uint32)*value >> 8;
	mbx_out[7] = (uint32)*value & 0xff;

	if (ec_mbxsend(slave_nr, &mbx_out, EC_TIMEOUTSAFE) <= 0) {
		ERR("failed to send mbx (gripper:%d, instr_nr=%d, param_nr=%d, slave_nr=%d, bank_nr=%d, value=%d",
		    gripper, instr_nr, param_nr, slave_nr, bank_nr, *value);
		goto out;
	}

	if (ec_mbxreceive(slave_nr, &mbx_in, EC_TIMEOUTSAFE) <= 0) {
		ERR("failed to receive mbx (request: gripper:%d, instr_nr=%d, param_nr=%d, slave_nr=%d, bank_nr=%d, value=%d",
		    gripper, instr_nr, param_nr, slave_nr, bank_nr, *value);
		goto out;
	}

	*value = (mbx_in[4] << 24 | mbx_in[5] << 16 | mbx_in[6] << 8 | mbx_in[7]);

	if(((int) mbx_in[2]) != 100) {
		ERR("receiving mbx failed., addr=%d, module=%d, status=%d, command=%d, value=%d",
		    mbx_in[0], mbx_in[1], mbx_in[2], mbx_in[3], *value);
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
	if(!send_mbx(0, GAP, param_nr, slave_nr, 0, &rdval)) {
		ERR("slave %d, failed to read param=%d", slave_nr, param_nr);
		goto out;
	}

	if (rdval != val)
		DBG("set_param: slave=%d, param=%d not yet set, trying to set.", slave_nr, param_nr);

	/* set param */
	if(!send_mbx(0, SAP, param_nr, slave_nr, 0, &val)) {
		ERR("failed to set slave=%d param=%d to val=%d. Password required?", slave_nr, param_nr, val);
		goto out;
	}

	// re-read and check
	if(!send_mbx(0, GAP, param_nr, slave_nr, 0, &rdval)) {
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
		if(!send_mbx(0, GAP, MAX_CURRENT, ainf->jnt_inf[i].slave_idx, 0, &val)) {
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

	interface = (char *) ubx_config_get_data_ptr(b, "if", &interface_len);

	if(strncmp(interface, "", interface_len)==0) {
		ERR("config interface unset");
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


static void youbot_cleanup(ubx_block_t *b)
{
	free((struct youbot_info*) b->private_data);
}

static int youbot_start(ubx_block_t *b)
{
	return 0; /* Ok */
}

static void youbot_step(ubx_block_t *b) {
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
	.step = youbot_step,
	.cleanup = youbot_cleanup,
};

static int youbot_mod_init(ubx_node_info_t* ni)
{
	DBG(" ");
	return ubx_block_register(ni, &youbot_comp);
}

static void youbot_mod_cleanup(ubx_node_info_t *ni)
{
	DBG(" ");
	ubx_block_unregister(ni, "youbot/youbot_driver");
}

module_init(youbot_mod_init)
module_cleanup(youbot_mod_cleanup)
