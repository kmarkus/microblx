
#include "ramp.h"
#include <math.h>

/* define a structure for holding the block local state. By assigning an
 * instance of this struct to the block private_data pointer (see init), this
 * information becomes accessible within the hook functions.
 */
struct ramp_info
{
	double cur;
	double slope;
	
	/* this is to have fast access to ports for reading and writing, without
	 * needing a hash table lookup */
	struct ramp_port_cache ports;
};

/* init */
int ramp_init(ubx_block_t *b)
{
	int ret = -1;
	struct ramp_info *inf;
	unsigned int len;

	/* allocate memory for the block local state */
	if ((inf = (struct ramp_info*)calloc(1, sizeof(struct ramp_info)))==NULL) {
		ERR("ramp: failed to alloc memory");
		ret=EOUTOFMEM;
		goto out;
	}
	b->private_data=inf;
	update_port_cache(b, &inf->ports);

	inf->cur = *((double*) ubx_config_get_data_ptr(b, "start", &len));
	inf->slope = *((double*) ubx_config_get_data_ptr(b, "slope", &len));

	inf->slope = (fabs(inf->slope) > 10e-6) ? inf->slope : 1;
	
	ret=0;
out:
	return ret;
}

/* start */
int ramp_start(ubx_block_t *b)
{
	struct ramp_info *inf = (struct ramp_info*) b->private_data;
	DBG("start=%f, slope=%f", inf->cur, inf->slope);
	return 0;
}

/* stop */
void ramp_stop(ubx_block_t *b)
{
	/* struct ramp_info *inf = (struct ramp_info*) b->private_data; */
}

/* cleanup */
void ramp_cleanup(ubx_block_t *b)
{
	free(b->private_data);
}

/* step */
void ramp_step(ubx_block_t *b)
{
	struct ramp_info *inf = (struct ramp_info*) b->private_data;

	inf->cur += inf->slope;
	DBG("cur: %f", inf->cur);
	write_out(inf->ports.out, &inf->cur);
}

