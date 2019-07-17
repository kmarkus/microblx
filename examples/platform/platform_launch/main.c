#include <ubx.h>

#define WEBIF_PORT "8810"
#define DOUBLE_STR "double"
#include "ptrig_config.h"
#include "ptrig_period.h"
#include "signal.h"
int main()
{
  int len, ret=EXIT_FAILURE;
  ubx_node_info_t ni;
  ubx_block_t *plat1, *control1, *logger1, *ptrig1, *webif, *fifo_vel, *fifo_pos;
  ubx_data_t *d;

  /* initalize the node */
  ubx_node_init(&ni, "platform_and_control");
  const char *modules[8];
  modules[0]= "/usr/local/lib/ubx/0.6/stdtypes.so";
  modules[1]= "/usr/local/lib/ubx/0.6/stattypes.so";
  modules[2]= "/usr/local/lib/ubx/0.6/ptrig.so";
  modules[3]= "/usr/local/lib/ubx/0.6/platform_2dof.so";
  modules[4]= "/usr/local/lib/ubx/0.6/platform_2dof_control.so";
  modules[5]= "/usr/local/lib/ubx/0.6/webif.so";
  modules[6]= "/usr/local/lib/ubx/0.6/logger.so";
  modules[7]= "/usr/local/lib/ubx/0.6/lfds_cyclic.so";
  /* load modules */
  for (int i=0; i<8;i++)
    if(ubx_module_load(&ni, modules[i]) != 0){
        printf("fail to load %s",modules[i]);
        goto out;
      }


  //create cblocks
  if((plat1 = ubx_block_create(&ni, "platform_2dof", "plat1"))==NULL){
      printf("fail to create plat1");
      goto out;
    }
  if((control1 = ubx_block_create(&ni, "platform_2dof_control", "control1"))==NULL){
      printf("fail to create control1");
      goto out;
    }
  if((logger1 = ubx_block_create(&ni, "logging/file_logger", "logger1"))==NULL){
      printf("fail to create logger1");
      goto out;
    }
  if((ptrig1 = ubx_block_create(&ni, "std_triggers/ptrig", "ptrig1"))==NULL){
      printf("fail to create ptrig1");
      goto out;
    }
  if((webif = ubx_block_create(&ni, "webif/webif", "webif1"))==NULL){
      printf("fail to create webif1");
      goto out;
    }
  //crate iblocks
  if((fifo_pos = ubx_block_create(&ni, "lfds_buffers/cyclic", "fifo_pos"))==NULL){
      printf("fail to create fifo_pos");
      goto out;
    }
  if((fifo_vel = ubx_block_create(&ni, "lfds_buffers/cyclic", "fifo_vel"))==NULL){
      printf("fail to create fifo_vel");
      goto out;
    }

  //configuration of blocks
  //configuration of web interface
  d = ubx_config_get_data(webif, "port");
  len = strlen(WEBIF_PORT)+1;

  /* resize the char array as necessary and copy the port string */
  ubx_data_resize(d, len);
  strncpy((char *)d->data, WEBIF_PORT, len);

  d = ubx_config_get_data(plat1, "initial_position");
  ((double*)d->data)[0]=1.1;
  ((double*)d->data)[1]=0.1;
  d = ubx_config_get_data(plat1, "joint_velocity_limits");
  ((double*)d->data)[0]=0.5;
  ((double*)d->data)[1]=0.5;
  d = ubx_config_get_data(control1, "gain");
  *((double*)d->data)=0.12;
  d = ubx_config_get_data(control1, "target_pos");
  ((double*)d->data)[0]=4.5;
  ((double*)d->data)[1]=4.52;

  //logger config
  d=ubx_config_get_data(logger1, "filename");
  char filename[]="/tmp/platform_time.log";
  len = strlen(filename)+1;
  ubx_data_resize(d, len);
  strncpy((char *)d->data, filename, len);

  d=ubx_config_get_data(logger1, "report_conf");
  char report_conf[]="{{ blockname='plat1', portname='pos'},{ blockname='control1', portname='commanded_vel'}}";
  len = strlen(report_conf)+1;
  ubx_data_resize(d, len);
  strncpy((char *)d->data, report_conf, len);

  d=ubx_config_get_data(logger1, "separator");
  char separator[]=",";
  len = strlen(separator)+1;
  ubx_data_resize(d, len);
  strncpy((char *)d->data, separator, len);

  //ptrig config
  /*{ name="ptrig1", config = {
     period = {sec=1, usec=0 },
     sched_policy="SCHED_OTHER",
     sched_priority=0,
     trig_blocks={ { b="#plat1", num_steps=1, measure=1 },
                   { b="#control1", num_steps=1, measure=1 },
                   { b="#logger1", num_steps=1, measure=0 },
                   { b="#logger_time", num_steps=1, measure=0 }  } } }  */
  //struct ptrig_period p;
  //p.sec=1;
  //p.usec=14;
  d=ubx_config_get_data(ptrig1, "period");
  ubx_data_resize(d, 1);
  //*((struct ptrig_period*)d->data)=p;
  ((struct ptrig_period*)d->data)->sec=1;
  ((struct ptrig_period*)d->data)->usec=14;

  d=ubx_config_get_data(ptrig1, "sched_policy");
  char policy[]="SCHED_OTHER";
  len = strlen(policy)+1;
  ubx_data_resize(d, len);
  strncpy((char *)d->data, policy, len);
  d=ubx_config_get_data(ptrig1, "sched_priority");
  *((int*)d->data)=0;

  d=ubx_config_get_data(ptrig1, "trig_blocks");
  len= 3;
  ubx_data_resize(d, len);
  printf("data size trig blocks: %li\n",d->type->size);
  ((struct ptrig_config*)d->data)[0].b= plat1;//ubx_block_get(&ni, "plat1")
  ((struct ptrig_config*)d->data)[0].num_steps = 1;
  ((struct ptrig_config*)d->data)[0].measure = 0;
  ((struct ptrig_config*)d->data)[1].b= control1;
  ((struct ptrig_config*)d->data)[1].num_steps = 1;
  ((struct ptrig_config*)d->data)[1].measure = 0;
  ((struct ptrig_config*)d->data)[2].b= logger1;
  ((struct ptrig_config*)d->data)[2].num_steps = 1;
  ((struct ptrig_config*)d->data)[2].measure = 0;




  // i-block config
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

  //connections
  ubx_port_t* plat1_pos=ubx_port_get(plat1,"pos");
  ubx_port_t* control1_measured_pos=ubx_port_get(control1,"measured_pos");
  ubx_port_t* control1_commanded_vel=ubx_port_get(control1,"commanded_vel");
  ubx_port_t* plat1_desired_vel=ubx_port_get(plat1,"desired_vel");

  ubx_port_connect_out(plat1_pos,fifo_pos);
  ubx_port_connect_in(control1_measured_pos,fifo_pos);
  ubx_port_connect_out(control1_commanded_vel,fifo_vel);
  ubx_port_connect_in(plat1_desired_vel,fifo_vel);




  /* init and start the block */
  if(ubx_block_init(webif) != 0) {
      ERR("failed to init webif");
      goto out;
    }
  if(ubx_block_init(plat1) != 0) {
      ERR("failed to init plat1");
      goto out;
    }
  if(ubx_block_init(control1) != 0) {
      ERR("failed to init control1");
      goto out;
    }

  if(ubx_block_init(fifo_pos) != 0) {
      ERR("failed to init fifo_pos");
      goto out;
    }

  if(ubx_block_init(fifo_vel) != 0) {
      ERR("failed to init fifo_vel");
      goto out;
    }
  if(ubx_block_start(fifo_pos) != 0) {
      ERR("failed to start fifo_pos");
      goto out;
    }
  if(ubx_block_start(fifo_vel) != 0) {
      ERR("failed to start fifo_vel");
      goto out;
    }

  if(ubx_block_start(webif) != 0) {
      ERR("failed to start webif");
      goto out;
    }
  if(ubx_block_start(plat1) != 0) {
      ERR("failed to start plat1");
      goto out;
    }
  if(ubx_block_start(control1) != 0) {
      ERR("failed to start control1");
      goto out;
    }


  if(ubx_block_init(ptrig1) != 0) {
      ERR("HERE failed to init ptrig1");
      goto out;
    }
  if(ubx_block_start(ptrig1) != 0) {
      ERR("failed to start ptrig1");
      goto out;
    }
  if(ubx_block_init(logger1) != 0) {
      ERR("HERE failed to init logger1");
      goto out;
    }
  if(ubx_block_start(logger1) != 0) {
      ERR("failed to start logger1");
      goto out;
    }

  sigset_t set;
  int sig;

  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  pthread_sigmask(SIG_BLOCK, &set, NULL);
  sigwait(&set, &sig);

  //printf("webif block lauched on port  hit enter to quit\n");
  //getchar();

  ret=EXIT_SUCCESS;
out:
  /* this cleans up all blocks and unloads all modules */
  ubx_node_cleanup(&ni);
  exit(ret);
}
