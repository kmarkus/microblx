#include "ramp.h"

/* define a structure for holding the block local state. By assigning an
 * instance of this struct to the block private_data pointer (see init), this
 * information becomes accessible within the hook functions.
 */
struct ramp_info {
	RAMP_T *cur;
	const RAMP_T *start;
	const RAMP_T *slope;

	long data_len;

	struct ramp_port_cache ports;
};

def_port_writers(write_ramp, RAMP_T)

/* init */
int ramp_init(ubx_block_t *b)
{
	int ret = -1;
	long len;
	const long *data_len;
	struct ramp_info *inf;

	/* allocate memory for the block local state */
	inf = calloc(1, sizeof(struct ramp_info));
	if (inf == NULL) {
		ubx_err(b, "ramp: failed to alloc memory");
		ret = EOUTOFMEM;
		goto out;
	}

	b->private_data = inf;
	update_port_cache(b, &inf->ports);

	/* handle data_len conf */
	if((len = cfg_getptr_long(b, "data_len", &data_len)) < 0)
		goto out;

	inf->data_len = (len > 0) ? *data_len : 1;

	/* resize ports */
	if (ubx_outport_resize(inf->ports.out, inf->data_len) != 0)
		goto out;

	inf->cur = calloc(inf->data_len, sizeof(RAMP_T));

	if (inf->cur == NULL) {
		ubx_err(b, "EOUTOFMEM: failed to alloc buffer");
		goto out;
	}

	/* start config */
	len = ubx_config_get_data_ptr(b, "start", (void **)&inf->start);
	if (len < 0)
		goto out_free;

	if(len > 0 && len != inf->data_len) {
		ubx_err(b, "EINVALID_CONFIG_LEN: start: actual: %lu, expected %lu",
			len, inf->data_len);
		goto out_free;
	}

	inf->start = (len==0) ? NULL : inf->start;

	/* slope config */
	len = ubx_config_get_data_ptr(b, "slope", (void **)&inf->slope);
	if (len < 0)
		goto out_free;

	if (len == 0) {
		ubx_err(b, "EINVALID_CONFIG: mandatory config slope is unset");
		goto out_free;
	}


	if(len > 0 && len != inf->data_len) {
		ubx_err(b, "EINVALID_CONFIG_LEN: start: actual: %lu, expected %lu",
			len, inf->data_len);
		goto out_free;
	}

	ret = 0;
	goto out;

out_free:
	free(inf->cur);
out:
	return ret;
}


/* start */
int ramp_start(ubx_block_t *b)
{
	struct ramp_info *inf = (struct ramp_info *)b->private_data;

	/* initialize cur to start */
	if (inf->start)
		memcpy(inf->cur, inf->start, inf->data_len * sizeof(RAMP_T));

	return 0;
}

/* cleanup */
void ramp_cleanup(ubx_block_t *b)
{
	struct ramp_info *inf = (struct ramp_info *)b->private_data;
	free(inf->cur);
	free(b->private_data);
}

/* step */
void ramp_step(ubx_block_t *b)
{
	struct ramp_info *inf = (struct ramp_info *)b->private_data;

	write_ramp_array(inf->ports.out, inf->cur, inf->data_len);

	for (int i=0; i<inf->data_len; i++)
		inf->cur[i] += inf->slope[i];
}
