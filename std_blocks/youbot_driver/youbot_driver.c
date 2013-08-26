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

/* types */
#include "types/control_modes.h"

/* block state */
struct youbot_info {
	uint8_t io_map[4096];

	/* statistics */
	uint64_t overcurrent;
	uint64_t undervoltage;
	uint64_t overvoltage;
	uint64_t overtemp;
	uint64_t hall_err;
	uint64_t encoder_err;
	uint64_t sine_comm_init_err;
	uint64_t emergency_stop;
	uint64_t ec_timeout;
	uint64_t i2t_exceeded;
};

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

static int youbot_init(ubx_block_t *b)
{
	int ret=-1, i;
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
		ERR("ec_init failed (IF: %s), sufficient rights, correct interface?", interface);
		goto out_free;
	}
	
	if(ec_config(TRUE, &inf->io_map) <= 0) {
		ERR("ec_config failed  (IF: %s)", interface);
		goto out_free;
	} else {
		DBG("detected %d slaves", ec_slavecount);
	}

	/* check for base, base is mandatory for a youbot */
	if(ec_slavecount >= YOUBOT_NR_OF_BASE_SLAVES) {
		if(strcmp(ec_slave[1].name, YOUBOT_WHEEL_PWRB_NAME) != 0) {
			ERR("expected base powerboard (%s), found %s",
			    YOUBOT_WHEEL_PWRB_NAME, ec_slave[1].name);
			goto out_free;
		}
		
		for(i=2; i<=YOUBOT_NR_OF_WHEELS+2; i++) {
			if(strcmp(ec_slave[i].name, YOUBOT_WHEEL_CTRL_NAME) != 0 ||
			   strcmp(ec_slave[i].name, YOUBOT_WHEEL_CTRL_NAME2) != 0) {
				ERR("expected base controller (%s or %s), found %s",
				    YOUBOT_WHEEL_CTRL_NAME, YOUBOT_WHEEL_CTRL_NAME2, ec_slave[i].name);
				goto out_free;
			}
		}
	}

	/* check for arm */

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
