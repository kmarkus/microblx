#include <ubx.h>
#include <ubx/trig_utils.h>

#define WEBIF_PORT "8810"
#define DOUBLE_STR "double"

#include "ptrig_period.h"
#include "signal.h"

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
	int len, ret=EXIT_FAILURE;
	ubx_node_t nd;
	ubx_block_t *plat1, *control1, *ptrig1, *webif, *fifo_vel, *fifo_pos;
	ubx_data_t *d;

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
 	if((ptrig1 = ubx_block_create(&nd, "std_triggers/ptrig", "ptrig1"))==NULL){
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "fail to create ptrig1");
		goto out;
	}
	if((webif = ubx_block_create(&nd, "webif/webif", "webif1"))==NULL){
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "fail to create webif1");
		goto out;
	}
	/* create iblocks */
	if((fifo_pos = ubx_block_create(&nd, "lfds_buffers/cyclic", "fifo_pos"))==NULL){
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "fail to create fifo_pos");
		goto out;
	}
	if((fifo_vel = ubx_block_create(&nd, "lfds_buffers/cyclic", "fifo_vel"))==NULL){
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "fail to create fifo_vel");
		goto out;
	}

	/* configuration of blocks configuration of web interface */
	d = ubx_config_get_data(webif, "port");
	len = strlen(WEBIF_PORT)+1;

	/* resize the char array as necessary and copy the port string */
	ubx_data_resize(d, len);
	strncpy((char *)d->data, WEBIF_PORT, len);

	d = ubx_config_get_data(plat1, "initial_position");
	ubx_data_resize(d, 2);
	((double*)d->data)[0]=1.1;
	((double*)d->data)[1]=0.1;
	d = ubx_config_get_data(plat1, "joint_velocity_limits");
	ubx_data_resize(d, 2);
	((double*)d->data)[0]=0.5;
	((double*)d->data)[1]=0.5;
	d = ubx_config_get_data(control1, "gain");
	ubx_data_resize(d, 2);
	*((double*)d->data)=0.12;
	d = ubx_config_get_data(control1, "target_pos");
	ubx_data_resize(d, 2);
	((double*)d->data)[0]=4.5;
	((double*)d->data)[1]=4.52;

	/* ptrig config */
	d=ubx_config_get_data(ptrig1, "period");
	ubx_data_resize(d, 1);

	((struct ptrig_period*)d->data)->sec=1;
	((struct ptrig_period*)d->data)->usec=14;

	d=ubx_config_get_data(ptrig1, "sched_policy");
	char policy[]="SCHED_OTHER";
	ubx_data_resize(d, strlen(policy)+1);
	strcpy((char *)d->data, policy);

	d=ubx_config_get_data(ptrig1, "sched_priority");
	ubx_data_resize(d, 1);
	*((int*)d->data)=0;

	d=ubx_config_get_data(ptrig1, "num_chains");
	ubx_data_resize(d, 1);
	*((int*)d->data)=1;

	if(ubx_block_init(ptrig1) != 0) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "HERE failed to init ptrig1");
		goto out;
	}

	d=ubx_config_get_data(ptrig1, "chain0");
	len = 2;
	ubx_data_resize(d, len);
	((struct ubx_triggee*)d->data)[0].b = plat1;
	/*As alternative: the block can be retieved by name:
	 *(struct ubx_triggee*)d->data)[0].b= ubx_block_get(&nd, "plat1")*/

	((struct ubx_triggee*)d->data)[0].num_steps = 1;
	((struct ubx_triggee*)d->data)[1].b= control1;
	((struct ubx_triggee*)d->data)[1].num_steps = 1;

	/* i-block configuration */
	len = strlen(DOUBLE_STR)+1;
	d = ubx_config_get_data(fifo_pos, "type_name");
	ubx_data_resize(d, len);
	strncpy(d->data, DOUBLE_STR, len);
	d = ubx_config_get_data(fifo_pos, "data_len");
	ubx_data_resize(d, 1);
	*((uint32_t*)d->data)=2;
	d = ubx_config_get_data(fifo_pos, "buffer_len");
	ubx_data_resize(d, 1);
	*((uint32_t*)d->data)=1;

	d = ubx_config_get_data(fifo_vel, "type_name");
	ubx_data_resize(d, len);
	strncpy(d->data, DOUBLE_STR, len);
	d = ubx_config_get_data(fifo_vel, "data_len");
	ubx_data_resize(d, 1);
	*((uint32_t*)d->data)=2;
	d = ubx_config_get_data(fifo_vel, "buffer_len");
	ubx_data_resize(d, 1);
	*((uint32_t*)d->data)=1;

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

	sigset_t set;
	int sig;

	/* stop on SIGINT (ctrl + c) */
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	pthread_sigmask(SIG_BLOCK, &set, NULL);
	sigwait(&set, &sig);


	ret=EXIT_SUCCESS;
out:
	printf("EXIT\n");
	/* this cleans up all blocks and unloads all modules */
	ubx_node_rm(&nd);
	exit(ret);
}

