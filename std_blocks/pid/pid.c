/*
 * PID controller function block
 * Copyright (C) 2020 Markus Klotzbuecher <mk@mkio.de>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define DEBUG

#include "pid.h"

/* def_read_dynarr_fun(read_doubles, double); */
def_port_writers(write_double, double)
def_port_readers(read_double, double)

/* block local state */
struct pid_info
{
	long data_len;	/* array length of in and out data */

	double *out, *msr, *des;
	double *err, *err_prev, *integ, *deriv;

	int has_err_prev;

	const double *kp, *ki, *kd;
	struct pid_port_cache ports;
};

/* init */
int pid_init(ubx_block_t *b)
{
	int ret = -1;
	long len;
	const long *data_len;

	struct pid_info *inf;

	/* allocate memory for the block local state */
	if ((inf = calloc(1, sizeof(struct pid_info)))==NULL) {
		ubx_err(b, "pid: failed to alloc pid_info");
		ret=EOUTOFMEM;
		goto out;
	}
	b->private_data = inf;
	update_port_cache(b, &inf->ports);

	/* config data_len */
	len = cfg_getptr_long(b, "data_len", &data_len);
	if (len < 0)
		goto out;

	inf->data_len = (len > 0) ? *data_len : 1;

	/* resize ports */
	if (ubx_inport_resize(inf->ports.msr, inf->data_len) ||
	    ubx_inport_resize(inf->ports.des, inf->data_len) ||
	    ubx_outport_resize(inf->ports.out, inf->data_len)) {
		goto out;
	}

	/* Kp */
	len = cfg_getptr_double(b, "Kp", &inf->kp);
	if (len < 0)
		goto out;

	if (len > 0 && len != inf->data_len) {
		ubx_err(b, "EINVALID_CONFIG_LEN: Kp: actual: %lu, expected %lu",
			len, inf->data_len);
		goto out;
	}

	/* Ki */
	len = cfg_getptr_double(b, "Ki", &inf->ki);
	if (len < 0)
		goto out;

	if (len > 0 && len != inf->data_len) {
		ubx_err(b, "EINVALID_CONFIG_LEN: Ki: actual: %lu, expected %lu",
			len, inf->data_len);
		goto out;
	}

	/* Kd */
	len = cfg_getptr_double(b, "Kd", &inf->kd);
	if (len < 0)
		goto out;

	if (len > 0 && len != inf->data_len) {
		ubx_err(b, "EINVALID_CONFIG_LEN: Kd: actual: %lu, expected %lu",
			len, inf->data_len);
		goto out;
	}

	/* allocate buffers */
	inf->out = calloc(inf->data_len, sizeof(double));
	inf->msr = calloc(inf->data_len, sizeof(double));
	inf->des = calloc(inf->data_len, sizeof(double));
	inf->err = calloc(inf->data_len, sizeof(double));
	inf->err_prev = calloc(inf->data_len, sizeof(double));
	inf->integ = calloc(inf->data_len, sizeof(double));
	inf->deriv = calloc(inf->data_len, sizeof(double));

	if (! (inf->out && inf->msr && inf->des && inf->err &&
	       inf->err_prev && inf->integ && inf->deriv)) {
		ubx_err(b, "EOUTOFMEM: failed to allocate buffers");
		goto out_err;
	}

	ret=0;
	goto out;

out_err:
	if (inf->out) free(inf->out);
	if (inf->msr) free(inf->msr);
	if (inf->des) free(inf->des);
	if (inf->err) free(inf->err);
	if (inf->err_prev) free(inf->err_prev);
	if (inf->integ) free(inf->integ);
	if (inf->deriv) free(inf->deriv);
out:
	return ret;
}

/* cleanup */
void pid_cleanup(ubx_block_t *b)
{
	struct pid_info *inf = (struct pid_info*) b->private_data;

	free(inf->out);
	free(inf->msr);
	free(inf->des);
	free(inf->err);
	free(inf->err_prev);
	free(inf->integ);
	free(inf->deriv);
	free(b->private_data);
}

/* step */
void pid_step(ubx_block_t *b)
{
	double *tmp;
	long len_msr, len_des;
	struct pid_info *inf = (struct pid_info*) b->private_data;

	len_msr = read_double_array(inf->ports.msr, inf->msr, inf->data_len);
	len_des = read_double_array(inf->ports.des, inf->des, inf->data_len);

	if (len_msr < 0) {
		ubx_err(b, "ENODATA: no data on port msr");
		goto out;
	}

	if (len_des < 0) {
		ubx_err(b, "ENODATA: no data on port des");
		goto out;
	}

	if (len_msr != inf->data_len) {
		ubx_err(b, "msr value has wrong length (got: %lu, expected: %lu)",
			len_msr, inf->data_len);
		goto out;
	}

	if (len_des != inf->data_len) {
		ubx_err(b, "des value has wrong length (got: %lu, expected: %lu)",
			len_des, inf->data_len);
		goto out;
	}

	/* compute error and proportional component */
	for (int i=0; i<inf->data_len; i++) {
		inf->err[i] = inf->des[i] - inf->msr[i];
		inf->out[i] = inf->kp[i] * inf->err[i];
	}

	/* add integral component */
	if (inf->ki) {
		for (int i=0; i<inf->data_len; i++) {
			inf->integ[i] += inf->err[i];
			inf->out[i] += inf->ki[i] * inf->integ[i];
		}

	}

	/* add  */
	if (inf->kd && inf->has_err_prev) {
		for (int i=0; i<inf->data_len; i++) {
			inf->deriv[i] = inf->err[i] - inf->err_prev[i];
			inf->out[i] += inf->kd[i] * inf->deriv[i];
		}
	}

	write_double_array(inf->ports.out, inf->out, inf->data_len);

	/* swap ptrs */
	tmp = inf->err_prev;
	inf->err_prev = inf->err;
	inf->err = tmp;
	inf->has_err_prev = 1;
out:
	return;
}
