#include "platform_2dof_control.h"
#include <math.h> //fabs

/* edit and uncomment this:
 * UBX_MODULE_LICENSE_SPDX(...)
 */

/* define a structure for holding the block local state. By assigning an
 * instance of this struct to the block private_data pointer (see init), this
 * information becomes accessible within the hook functions.
 */
struct platform_2dof_control_info
{
  /* add custom block local data here */
  double gain;
  double target[2];
  /* this is to have fast access to ports for reading and writing, without
         * needing a hash table lookup */
  struct platform_2dof_control_port_cache ports;
};

/* init */
int platform_2dof_control_init(ubx_block_t *b)
{
  int ret = -1;
  long int len;
  struct platform_2dof_control_info *inf;

  /* allocate memory for the block local state */
  if ((inf = (struct platform_2dof_control_info*)calloc(1, sizeof(struct platform_2dof_control_info)))==NULL) {
       ubx_err(b,"platform_2dof_control: failed to alloc memory");
      ret=EOUTOFMEM;
      goto out;
    }
  b->private_data=inf;
  update_port_cache(b, &inf->ports);
  double *gain;
  double *target;
  if ((len = ubx_config_get_data_ptr(b, "gain",(void **)&gain)) < 0) {
       ubx_err(b,"failed to load gain");
      goto out;
    }
  inf->gain=*gain;
  if ((len = ubx_config_get_data_ptr(b, "target_pos",(void **)&target)) < 0) {
       ubx_err(b, "failed to load target_pos");
      goto out;
    }
  inf->target[0]=target[0];
  inf->target[1]=target[1];
  ret=0;

out:
  return ret;
}

/* start */
int platform_2dof_control_start(ubx_block_t *b)
{
  /* struct platform_2dof_control_info *inf = (struct platform_2dof_control_info*) b->private_data; */

  int ret = 0;
  return ret;
}

/* stop */
void platform_2dof_control_stop(ubx_block_t *b)
{
  /* struct platform_2dof_control_info *inf = (struct platform_2dof_control_info*) b->private_data; */
}

/* cleanup */
void platform_2dof_control_cleanup(ubx_block_t *b)
{
  /* struct platform_2dof_control_info *inf = (struct platform_2dof_control_info*) b->private_data; */
  free(b->private_data);
}

/* step */
void platform_2dof_control_step(ubx_block_t *b)
{
  /* struct platform_2dof_control_info *inf = (struct platform_2dof_control_info*) b->private_data; */
  int ret;
  struct platform_2dof_control_info *inf = (struct platform_2dof_control_info*) b->private_data;
  ubx_port_t* pos_port = ubx_port_get(b, "measured_pos");
  ubx_port_t* vel_port = ubx_port_get(b, "commanded_vel");
  double velocity[2];
  double pos[2];
  ret = read_measured_pos_2(pos_port,&pos);
  if (ret<=0) {//nodata
    }else{
      velocity[0]=(inf->target[0] -pos[0])*(inf->gain);
      velocity[1]=(inf->target[1] -pos[1])*(inf->gain);
    }

  write_commanded_vel_2(vel_port,&velocity);
}

