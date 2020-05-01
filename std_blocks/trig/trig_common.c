
#include "trig_common.h"

int write_tstats_to_profile_path(ubx_block_t *b, struct trig_info *trig_inf)
{
	int len;
	const char *profile_path;

	len = cfg_getptr_char(b, "tstats_profile_path", &profile_path);

	if (len < 0)
		ubx_err(b, "unable to retrieve config tstats_profile_path");

	if (len == 0)
		return 0;

	return trig_info_tstats_write(b, trig_inf, profile_path);
}


int trig_info_config(ubx_block_t *b, struct trig_info *trig_inf)
{
	int ret = -1;
	long len;
	const int *val;
	const double *val_double;
	double output_rate;

	/* tstats_mode */
	len = cfg_getptr_int(b, "tstats_mode", &val);
	assert(len >= 0);
	trig_inf->tstats_mode = (len > 0) ? *val : 0;

	/* tstats_output_rate */
	len = cfg_getptr_double(b, "tstats_output_rate", &val_double);
	assert(len >= 0);
	output_rate = (len > 0) ? *val_double : 0;

	/* tstats_skip_first */
	len = cfg_getptr_int(b, "tstats_skip_first", &val);
	assert(len >= 0);
	trig_inf->tstats_skip_first = (len > 0) ? *val : 0;

	/* trig_blocks */
	trig_inf->trig_list_len = cfg_getptr_trig_spec(b, "trig_blocks", &trig_inf->trig_list);
	assert(trig_inf->trig_list_len >= 0);

	trig_inf->p_tstats = ubx_port_get(b, "tstats");
	assert(trig_inf->p_tstats);

	/* initialize trig_info */
	ret = trig_info_init(trig_inf, b->name, output_rate);

	return ret;
}
