#include "platform_2dof.h"
#include "math.h"

/* define a structure for holding the block local state. By assigning an
 * instance of this struct to the block private_data pointer (see init), this
 * information becomes accessible within the hook functions.
 */
struct robot_state {
  double pos[2];
  double vel[2];
  double vel_limit[2];
};
double sign(double x)
{
  if(x > 0) return 1;
  if(x < 0) return -1;
  return 0;
}

struct platform_2dof_info
{
  /* add custom block local data here */
  struct robot_state r_state;
  struct ubx_timespec last_time;
  /* this is to have fast access to ports for reading and writing, without
         * needing a hash table lookup */
  struct platform_2dof_port_cache ports;
};

/* init */
int platform_2dof_init(ubx_block_t *b)
{
  int ret = -1;
  long int len;
  struct platform_2dof_info *inf;
  //  double pos_vec[2];
  double *pos_vec;

  /* allocate memory for the block local state */
  if ((inf = (struct platform_2dof_info*)calloc(1, sizeof(struct platform_2dof_info)))==NULL) {
      ERR("platform_2dof: failed to alloc memory");
      ret=EOUTOFMEM;
      goto out;
    }
  b->private_data=inf;
  update_port_cache(b, &inf->ports);

  //read configuration - initial position
  if ((len = ubx_config_get_data_ptr(b, "initial_position",(void **)&pos_vec)) < 0) {
      ERR("platform_2dof: failed to load initial_position");
      goto out;
    }
  inf->r_state.pos[0]=pos_vec[0];
  inf->r_state.pos[1]=pos_vec[1];
  if ((len = ubx_config_get_data_ptr(b, "joint_velocity_limits",(void **)&pos_vec)) < 0) {
      ERR("platform_2dof: failed to load joint_velocity_limits");
      goto out;
    }
  //read configuration - max velocity
  inf->r_state.vel_limit[0]=pos_vec[0];
  inf->r_state.vel_limit[1]=pos_vec[1];

  inf->r_state.vel[0]=inf->r_state.vel[1]=0.0;
  ret=0;
out:
  return ret;
}

/* start */
int platform_2dof_start(ubx_block_t *b)
{
  struct platform_2dof_info *inf = (struct platform_2dof_info*) b->private_data;
  MSG("%s", b->name);
  ubx_gettime(&(inf->last_time));



  int ret = 0;
  return ret;
}

/* stop */
void platform_2dof_stop(ubx_block_t *b)
{
  /* struct platform_2dof_info *inf = (struct platform_2dof_info*) b->private_data; */
  MSG("%s", b->name);
}

/* cleanup */
void platform_2dof_cleanup(ubx_block_t *b)
{
  /* struct platform_2dof_info *inf = (struct platform_2dof_info*) b->private_data; */
  MSG("%s", b->name);
  free(b->private_data);
}

/* step */
void platform_2dof_step(ubx_block_t *b)
{
  int32_t ret;
  double velocity[2];
  struct ubx_timespec current_time, difference;
  struct platform_2dof_info *inf = (struct platform_2dof_info*) b->private_data;
  //compute time from last call
  ubx_gettime(&current_time);
  ubx_ts_sub(&current_time,&(inf->last_time),&difference);
  inf->last_time=current_time;
  double time_passed= ubx_ts_to_double(&difference);

  //read velocity from port
  ret = read_desired_vel_2(inf->ports.desired_vel, &velocity);
  if (ret<=0){ //nodata
      velocity[0]=velocity[1]=0.0;
      ERR("no velocity data");
    }

  for (int i=0;i<2;i++){// saturate and integrate velocity
      velocity[i]=fabs(velocity[i])> inf->r_state.vel_limit[i]? sign(velocity[i])*(inf->r_state.vel_limit[i]):velocity[i];
      inf->r_state.pos[i]+=velocity[i]*time_passed;}
  //write position in the port
  write_pos_2(inf->ports.pos,&(inf->r_state.pos));

}

