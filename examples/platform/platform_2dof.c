#undef UBX_DEBUG /* ubx_debug is compiled out if undefined */

#include "platform_2dof.h"
#include <math.h>

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

	/* this is to have fast access to ports for reading
         * and writing, without needing a hash table lookup */
	struct platform_2dof_port_cache ports;
};

int platform_2dof_init(ubx_block_t *b)
{
	long len;
	struct platform_2dof_info *inf;
	const double *pos_vec;

	/* allocate memory for the block local state */
	if ((inf = calloc(1, sizeof(struct platform_2dof_info)))==NULL) {
		ubx_err(b,"platform_2dof: failed to alloc memory");
		return EOUTOFMEM;
	}

	b->private_data=inf;
	update_port_cache(b, &inf->ports);

	/* read configuration - initial position */
	len = cfg_getptr_double(b, "initial_position", &pos_vec);

	/* this will never assert unless we made an error
	 * (e.g. mistyped the configuration name), since min/max
	 * checking will catch misconfigurations before we get
	 * here. */
	assert(len==2);

	inf->r_state.pos[0] = pos_vec[0];
	inf->r_state.pos[1] = pos_vec[1];

	/* read configuration - max velocity */
	len = cfg_getptr_double(b, "joint_velocity_limits", &pos_vec);
	assert(len==2);

	inf->r_state.vel_limit[0] = pos_vec[0];
	inf->r_state.vel_limit[1] = pos_vec[1];
	inf->r_state.vel[0] = 0.0;
	inf->r_state.vel[1] = 0.0;

	return 0;
}

int platform_2dof_start(ubx_block_t *b)
{
	struct platform_2dof_info *inf = (struct platform_2dof_info*) b->private_data;
	ubx_info(b, "platform_2dof start");
	ubx_gettime(&inf->last_time);
	return 0;
}

/* cleanup */
void platform_2dof_cleanup(ubx_block_t *b)
{
        ubx_info(b, "%s", __func__);
	free(b->private_data);
}

void platform_2dof_step(ubx_block_t *b)
{
	int32_t ret;
	double velocity[2];
	struct ubx_timespec current_time, difference;
	struct platform_2dof_info *inf = (struct platform_2dof_info*) b->private_data;

	/* compute time from last call */
	ubx_gettime(&current_time);
	ubx_ts_sub(&current_time, &inf->last_time, &difference);
	inf->last_time = current_time;
	double time_passed = ubx_ts_to_double(&difference);

	/* read velocity from port */
	ret = read_double_array(inf->ports.desired_vel, velocity, 2);
	assert(ret>=0);

	if (ret == 0) { /* nodata */
		ubx_notice(b,"no velocity setpoint");
		velocity[0] = velocity[1] = 0.0;
	}

	for (int i=0; i<2; i++) {
		/* saturate and integrate velocity */
		velocity[i] =
			fabs(velocity[i]) > inf->r_state.vel_limit[i] ?
			sign(velocity[i]) * (inf->r_state.vel_limit[i]) : velocity[i];

		inf->r_state.pos[i] += velocity[i] * time_passed;
	}
	/* write position in the port */
	ubx_debug(b, "writing pos [%f, %f]",
		  inf->r_state.pos[0], inf->r_state.pos[1]);
	write_double_array(inf->ports.pos, inf->r_state.pos, 2);
}
