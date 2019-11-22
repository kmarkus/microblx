#include "ramp.h"
#include <math.h>

/* define a structure for holding the block local state. By assigning an
 * instance of this struct to the block private_data pointer (see init), this
 * information becomes accessible within the hook functions.
 */
struct ramp_info
{
	RAMP_T cur;
	RAMP_T slope;

	/* this is to have fast access to ports for reading and writing, without
	 * needing a hash table lookup */
	struct ramp_port_cache ports;
};

/* init */
int ramp_init(ubx_block_t *b)
{
	int ret = -1;
	long int len;
	const RAMP_T *val;
	struct ramp_info *inf;

	/* allocate memory for the block local state */
	if ((inf = (struct ramp_info*)calloc(1, sizeof(struct ramp_info)))==NULL) {
		ubx_err(b, "ramp: failed to alloc memory");
		ret=EOUTOFMEM;
		goto out;
	}

	b->private_data=inf;
	update_port_cache(b, &inf->ports);

	/* handle start configuration */
	if((len = ubx_config_get_data_ptr(b, "start", (void**) &val)) < 0)
		goto out;

	inf->cur = (len > 0) ? *val : 0;

	/* handle slope configuration */
	if((len = ubx_config_get_data_ptr(b, "slope", (void**) &val)) < 0)
		goto out;

	inf->slope = (len > 0) ? *val : 1;
	inf->slope = (fabs((double) inf->slope) > 10e-6) ? inf->slope : 1;

	ret=0;
out:
	return ret;
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
	ubx_debug(b, "cur: %g", (double) inf->cur);
	write_out(inf->ports.out, &inf->cur);
}
