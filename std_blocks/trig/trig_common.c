
#include "trig_common.h"

int trig_info_config(ubx_block_t *b, struct trig_info *trig_inf)
{
	int ret = -1;
	long len, trig_list_len;
	const int *val;
	const double *val_double;
	int tstats_mode;
	const char *profile_path;

	double output_rate;
	ubx_port_t *p_tstats;
	struct ubx_trig_spec *trig_spec;

	/* tstats_mode */
	len = cfg_getptr_int(b, "tstats_mode", &val);

	if (len < 0) {
		ubx_err(b, "unable to retrieve config tstats_mode");
		goto out;
	}

	tstats_mode = (len > 0) ? *val : 0;

	/* tstats_output_rate */
	len = cfg_getptr_double(b, "tstats_output_rate", &val_double);

	if (len < 0) {
		ubx_err(b, "unable to retrieve config tstats_output_rate");
		goto out;
	}

	output_rate = (len > 0) ? *val_double : 0;

	/* trig_blocks */
	trig_list_len = ubx_config_get_data_ptr(b, "trig_blocks", (void **)&trig_spec);

	if (trig_list_len < 0) {
		ubx_err(b, "unable to retrieve config trig_blocks");
		goto out;
	}

	/* profile path */
	len = cfg_getptr_char(b, "tstats_profile_path", &profile_path);

	if (len < 0) {
		ubx_err(b, "unable to retrieve config tstats_profile_path");
		goto out;
	}

	/* tstats port */
	p_tstats = ubx_port_get(b, "tstats");

	/* initialize trig_info */
	ret = trig_info_init(trig_inf, NULL, tstats_mode,
			     trig_spec, trig_list_len,
			     output_rate, p_tstats, profile_path);

out:
	return ret;
}
