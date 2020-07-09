#include <ubx/ubx.h>
#include <ubx/trig_utils.h>

#define WEBIF_PORT "8810"

#include "ptrig_period.h"
#include "signal.h"

def_cfg_set_fun(cfg_set_ptrig_period, struct ptrig_period);
def_cfg_set_fun(cfg_set_ubx_triggee, struct ubx_triggee);

static const char* modules[] = {
	"/usr/local/lib/ubx/0.9/stdtypes.so",
	"/usr/local/lib/ubx/0.9/ptrig.so",
	"/usr/local/lib/ubx/0.9/platform_2dof.so",
	"/usr/local/lib/ubx/0.9/platform_2dof_control.so",
	"/usr/local/lib/ubx/0.9/webif.so",
	"/usr/local/lib/ubx/0.9/lfds_cyclic.so",
};

int main()
{
	int ret = EXIT_FAILURE;
	ubx_node_t nd;
	ubx_block_t *plat1, *control1, *ptrig1, *webif, *fifo_vel, *fifo_pos;

	/* initalize the node */
	nd.loglevel = 7;
	ubx_node_init(&nd, "platform_and_control", 0);

	/* load modules */
	for (unsigned int i=0; i<ARRAY_SIZE(modules); i++) {
		if(ubx_module_load(&nd, modules[i]) != 0){
			ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "fail to load  module %s %i",modules[i], i);
			goto out;
		}
	}

	/* create cblocks */
	if((plat1 = ubx_block_create(&nd, "platform_2dof", "plat1"))==NULL){
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "fail to create plat1");
		goto out;
	}
	if((control1 = ubx_block_create(&nd, "platform_2dof_control", "control1"))==NULL){
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "fail to create control1");
		goto out;
	}
 	if((ptrig1 = ubx_block_create(&nd, "ubx/ptrig", "ptrig1"))==NULL){
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "fail to create ptrig1");
		goto out;
	}
	if((webif = ubx_block_create(&nd, "webif/webif", "webif1"))==NULL){
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "fail to create webif1");
		goto out;
	}
	/* create iblocks */
	if((fifo_pos = ubx_block_create(&nd, "ubx/lfds_cyclic", "fifo_pos"))==NULL){
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "fail to create fifo_pos");
		goto out;
	}
	if((fifo_vel = ubx_block_create(&nd, "ubx/lfds_cyclic", "fifo_vel"))==NULL){
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "fail to create fifo_vel");
		goto out;
	}

	/* webif port config */
	if (cfg_set_char(webif, "port", WEBIF_PORT, strlen(WEBIF_PORT))) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "failed to configure port_vel");
		goto out;
	}

	/* plat1 initial_position */
	const double initial_position[2] = { 1.1, 0.1 };

	if (cfg_set_double(plat1, "initial_position", initial_position, 2)) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "failed to configure initial_position");
		goto out;
	}

	/* joint_velocity_limits */
	const double joint_velocity_limits[2] = { 0.5, 0.5 };

	if (cfg_set_double(plat1, "joint_velocity_limits", joint_velocity_limits, 2)) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "failed to configure joint_velocity_limits");
		goto out;
	}

	/* gain */
	double gain = 0.12;

	if (cfg_set_double(control1, "gain", &gain, 1)) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd, __func__,  "failed to configure gain");
		goto out;
	}

	/* target_pos */
	const double target_pos[2] = { 4.5, 4.52 };

	if (cfg_set_double(control1, "target_pos", target_pos, 2)) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "failed to configure target_pos");
		goto out;
	}

	/* ptrig config */
	const struct ptrig_period period = { .sec=1, .usec=14 };

	if (cfg_set_ptrig_period(ptrig1, "period", &period, 1)) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "failed to configure ptrig_period");
		goto out;
	}

	/* sched_policy */
	const char sched_policy [] = "SCHED_OTHER";

	if (cfg_set_char(ptrig1, "sched_policy", sched_policy, strlen(sched_policy))) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "failed to configure sched_policy");
		goto out;
	}

	/* sched_priority */
	const int sched_priority = 0;

	if (cfg_set_int(ptrig1, "sched_priority", &sched_priority, 1)) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "failed to configure sched_priority");
		goto out;
	}

	/* ptrig needs to be initialized before configuring the
	 * chain0, since the chainX configs are dynamically added
	 * based on the `num_chains` configuration (unset it defaults
	 * to 1,. i.e. chain0) */

	if(ubx_block_init(ptrig1) != 0) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd, __func__,  "failed to init ptrig1");
		goto out;
	}

	/* chain0 */
	const struct ubx_triggee chain0[] = {
		{ .b = plat1, .num_steps = 1 },
		{ .b = control1, .num_steps = 1 },
	};

	if (cfg_set_ubx_triggee(ptrig1, "chain0", chain0, ARRAY_SIZE(chain0))) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "failed to configure chain0");
		goto out;
	}

	/* i-block configuration */
	const char type_name[] = "double";
	const uint32_t data_len = 2;
	const uint32_t buffer_len = 1;

	/* fifo_pos */
	if (cfg_set_char(fifo_pos, "type_name",	type_name, strlen(type_name)) ||
	    cfg_set_uint32(fifo_pos, "data_len", &data_len, 1) ||
	    cfg_set_uint32(fifo_pos, "buffer_len", &buffer_len, 1)) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "failed to configure fifo_pos iblock");
	}

	/* fifo_vel */
	if (cfg_set_char(fifo_vel, "type_name",	type_name, strlen(type_name)) ||
	    cfg_set_uint32(fifo_vel, "data_len", &data_len, 1) ||
	    cfg_set_uint32(fifo_vel, "buffer_len", &buffer_len, 1)) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "failed to configure fifo_pos iblock");
	}

	/* connections setup - first ports must be retrived and then connected */
	ubx_port_t* plat1_pos = ubx_port_get(plat1,"pos");
	ubx_port_t* control1_measured_pos = ubx_port_get(control1,"measured_pos");
	ubx_port_t* control1_commanded_vel = ubx_port_get(control1,"commanded_vel");
	ubx_port_t* plat1_desired_vel = ubx_port_get(plat1,"desired_vel");

	ubx_port_connect_out(plat1_pos,fifo_pos);
	ubx_port_connect_in(control1_measured_pos,fifo_pos);
	ubx_port_connect_out(control1_commanded_vel,fifo_vel);
	ubx_port_connect_in(plat1_desired_vel,fifo_vel);

	/* init and start blocks */
	if(ubx_block_init(webif) != 0) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "failed to init webif");
		goto out;
	}

	if(ubx_block_init(plat1) != 0) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "failed to init plat1");
		goto out;
	}

	if(ubx_block_init(control1) != 0) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "failed to init control1");
		goto out;
	}

	if(ubx_block_init(fifo_pos) != 0) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "failed to init fifo_pos");
		goto out;
	}

	if(ubx_block_init(fifo_vel) != 0) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "failed to init fifo_vel");
		goto out;
	}

	if(ubx_block_start(fifo_pos) != 0) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "failed to start fifo_pos");
		goto out;
	}

	if(ubx_block_start(fifo_vel) != 0) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "failed to start fifo_vel");
		goto out;
	}

	if(ubx_block_start(webif) != 0) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "failed to start webif");
		goto out;
	}

	if(ubx_block_start(plat1) != 0) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "failed to start plat1");
		goto out;
	}

	if(ubx_block_start(control1) != 0) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "failed to start control1");
		goto out;
	}

	if(ubx_block_start(ptrig1) != 0) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "failed to start ptrig1");
		goto out;
	}

	printf("started system,	webif @ http://localhost:%s\n", WEBIF_PORT);

	/* stop on SIGINT (ctrl + c) */
	ubx_wait_sigint(UINT_MAX);
	printf("shutting down\n");
	ret = EXIT_SUCCESS;
out:
	/* this cleans up all blocks and unloads all modules */
	ubx_node_rm(&nd);
	exit(ret);
}

