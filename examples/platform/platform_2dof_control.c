#include "platform_2dof_control.h"

#include <math.h>

struct platform_2dof_control_info
{
	/* add custom block local data here */
	double gain;
	double target[2];
	/* this is to have fast access to ports for reading and
         * writing, without needing a hash table lookup */
	struct platform_2dof_control_port_cache ports;
};

int platform_2dof_control_init(ubx_block_t *b)
{
	long len;
	const double *gain, *target;

	struct platform_2dof_control_info *inf;

	/* allocate memory for the block local state */
	inf = (struct platform_2dof_control_info*)
		calloc(1, sizeof(struct platform_2dof_control_info));

	if (inf == NULL) {
		ubx_err(b,"platform_2dof_control: failed to alloc memory");
		return EOUTOFMEM;
	}

	b->private_data=inf;
	update_port_cache(b, &inf->ports);

	len = cfg_getptr_double(b, "gain", &gain);
	assert(len==2);
	inf->gain = *gain;

	len = cfg_getptr_double(b, "target_pos", &target);
	assert(len==2);
	inf->target[0]=target[0];
	inf->target[1]=target[1];

	return 0;
}

void platform_2dof_control_cleanup(ubx_block_t *b)
{
	free(b->private_data);
}

void platform_2dof_control_step(ubx_block_t *b)
{
	int ret;
	double velocity[2];
	double pos[2];

	struct platform_2dof_control_info *inf =
		(struct platform_2dof_control_info*) b->private_data;

	ret = read_double_array(inf->ports.measured_pos, pos, 2);

	if (ret <= 0) {
		ubx_info(b, "unexpected NODATA");
	} else {
		velocity[0] = (inf->target[0] - pos[0]) * (inf->gain);
		velocity[1] = (inf->target[1] - pos[1]) * (inf->gain);
	}

	write_double_array(inf->ports.commanded_vel, velocity, 2);
}

