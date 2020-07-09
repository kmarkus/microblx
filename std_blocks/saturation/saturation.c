/*
 * saturation function block
 */

#define SAT_DOUBLE_T 1

#if SAT_FLOAT_T == 1
# define SAT_T float
#elif SAT_DOUBLE_T == 1
# define SAT_T double
#elif SAT_INT32_T == 1
# define SAT_T int32_t
# define BLOCK_NAME "ubx/sat_int32"
#elif SAT_INT64_T == 1
# define SAT_T int64_t
# define BLOCK_NAME "ubx/sat_int64"
#else
# error "unknown or missing type"
#endif

#ifndef BLOCK_NAME
# define BLOCK_NAME "ubx/saturation_" QUOTE(SAT_T)
#endif

#include <stdlib.h>
#include <string.h>
#include "ubx.h"

#define DATA_LEN 	"data_len"
#define LOWER_LIMITS 	"lower_limits"
#define UPPER_LIMITS 	"upper_limits"
#define POUT		"out"
#define PIN		"in"

char sat_meta[] = QUOTE(SAT_T) " saturation block";

ubx_proto_config_t sat_config[] = {
	{ .name = DATA_LEN, .type_name = "long", .doc = "data array length" },
	{ .name = LOWER_LIMITS, .type_name = QUOTE(SAT_T), .doc = "saturation lower limits" },
	{ .name = UPPER_LIMITS, .type_name = QUOTE(SAT_T), .doc = "saturation upper limits" },
	{ 0 },
};

ubx_proto_port_t sat_ports[] = {
	{ .name = PIN, .in_type_name = QUOTE(SAT_T), .doc = "input signal to saturate"  },
	{ .name = POUT, .out_type_name = QUOTE(SAT_T), .doc = "saturated output signal"  },
	{ 0 },
};

struct sat_info {
	long data_len;

	const SAT_T *lower_lim;
	const SAT_T *upper_lim;

	SAT_T *val;

	ubx_port_t *pout;
	ubx_port_t *pin;
};


int sat_init(ubx_block_t *b)
{
	int ret = EOUTOFMEM;
	long len;
	const long *ltmp;
	struct sat_info *inf;

	b->private_data = calloc(1, sizeof(struct sat_info));
	inf = (struct sat_info *)b->private_data;

	if (b->private_data == NULL) {
		ubx_err(b, "EOUTOFMEM: allocating sat_info failed");
		goto out;
	}

	inf = (struct sat_info *)b->private_data;

	/* data_len, default to 1 */
	len = cfg_getptr_long(b, DATA_LEN, &ltmp);
	assert(len >= 0);
	inf->data_len = (len > 0) ? *ltmp : 1;

	/* lower */
	len = ubx_config_get_data_ptr(b, LOWER_LIMITS, (void**) &inf->lower_lim);

	if (len != inf->data_len) {
		ubx_err(b, "EINVALID_CONFIG_LEN: %s is %lu but data_len is %lu",
			LOWER_LIMITS, len, inf->data_len);
		return -1;
	}

	/* upper */
	len = ubx_config_get_data_ptr(b, UPPER_LIMITS, (void**) &inf->upper_lim);

	if (len != inf->data_len) {
		ubx_err(b, "EINVALID_CONFIG_LEN: %s is %lu but data_len is %lu",
			UPPER_LIMITS, len, inf->data_len);
		return -1;
	}

	/* allocate memory for out value */
	inf->val = calloc(inf->data_len, sizeof(SAT_T));

	if (inf->val == NULL) {
		ubx_err(b, "EOUTOFMEM: allocating 'value' failed");
		goto out_free;
	}

	inf->pin =  ubx_port_get(b, PIN);
	inf->pout = ubx_port_get(b, POUT);

	/* resize ports */
	if (ubx_inport_resize(inf->pin, inf->data_len) ||
	    ubx_outport_resize(inf->pout, inf->data_len) != 0)
		return -1;

	ret = 0;
	goto out;

out_free:
	free(b->private_data);
out:
	return ret;
}

void sat_cleanup(ubx_block_t *b)
{
	struct sat_info *inf = (struct sat_info *)b->private_data;
	free(inf->val);
	free(inf);
}

void sat_step(ubx_block_t *b)
{
	long len;
	struct sat_info *inf = (struct sat_info *)b->private_data;

#if SAT_FLOAT_T == 1
	len = read_float_array(inf->pin, inf->val, inf->data_len);
#elif SAT_DOUBLE_T == 1
	len = read_double_array(inf->pin, inf->val, inf->data_len);
#elif SAT_INT32_T == 1
	len = read_int32_array(inf->pin, inf->val, inf->data_len);
#elif SAT_INT64_T == 1
	len = read_int64_array(inf->pin, inf->val, inf->data_len);
#endif

	if (len	== 0) {
		ubx_notice(b, "unexpected NODATA on port 'in'");
		return;
	}

	if (len != inf->data_len) {
		ubx_err(b, "in value has wrong length (got: %lu, expected: %lu)",
			len, inf->data_len);
		return;
	}

	for(long i=0; i<inf->data_len; i++) {
		inf->val[i] = (inf->val[i] > inf->upper_lim[i]) ? inf->upper_lim[i] : inf->val[i];
		inf->val[i] = (inf->val[i] < inf->lower_lim[i]) ? inf->lower_lim[i] : inf->val[i];
	}

#if SAT_FLOAT_T == 1
	write_float_array(inf->pout, inf->val, inf->data_len);
#elif SAT_DOUBLE_T == 1
	write_double_array(inf->pout, inf->val, inf->data_len);
#elif SAT_UINT32_T == 1
	write_uint32_array(inf->pout, inf->val, inf->data_len);
#elif SAT_INT32_T == 1
	write_int32_array(inf->pout, inf->val, inf->data_len);
#endif

}

ubx_proto_block_t sat_block = {
	.name = BLOCK_NAME,
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = sat_meta,
	.configs = sat_config,
	.ports = sat_ports,

	.init = sat_init,
	.cleanup = sat_cleanup,
	.step = sat_step,
};

int sat_mod_init(ubx_node_t *nd)
{
	return ubx_block_register(nd, &sat_block);
}

void sat_mod_cleanup(ubx_node_t *nd)
{
	ubx_block_unregister(nd, BLOCK_NAME);
}

UBX_MODULE_INIT(sat_mod_init)
UBX_MODULE_CLEANUP(sat_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
