/*
 * math double function block
 */

#undef UBX_DEBUG

#include <string.h>
#include <math.h>
#include "ubx.h"

char math_meta[] =
	" { doc='math functions from math.h',"
	"   realtime=true,"
	"}";

#define MATHFUNC_MAXLEN 16

struct mathfunc  {
	char name[MATHFUNC_MAXLEN];
	double (*f) (double);
};

#define _FUNC(FNAME) { .name=QUOTE(FNAME), .f=FNAME }

const struct mathfunc functions [] = {
	_FUNC(sin),  _FUNC(asin), _FUNC(sinh), _FUNC(asinh),
	_FUNC(cos),  _FUNC(acos), _FUNC(cosh), _FUNC(acosh),
	_FUNC(tan),  _FUNC(atan), _FUNC(tanh), _FUNC(atanh),
	_FUNC(cbrt),
	_FUNC(ceil),
	_FUNC(erf),  _FUNC(erfc),
	_FUNC(exp),  _FUNC(exp2), _FUNC(expm1),
	_FUNC(fabs), _FUNC(floor),
	_FUNC(j0),   _FUNC(j1),
	_FUNC(lgamma),
	_FUNC(log),  _FUNC(log10), _FUNC(log1p), _FUNC(log2), _FUNC(logb),
	_FUNC(nearbyint),
	_FUNC(rint),
	_FUNC(round),
	_FUNC(sqrt),
	_FUNC(tgamma),
	_FUNC(trunc),
	_FUNC(y0), _FUNC(y1),
};

#define CFUNC		"func"
#define CDATA_LEN	"data_len"
#define CMUL		"mul"
#define CADD		"add"

ubx_proto_config_t math_config[] = {
	{ .name = CFUNC, .type_name = "char", .doc = "math function to compute", .min=1 },
	{ .name = CDATA_LEN, .type_name = "long", .doc = "length of output data (def: 1)" },
	{ .name = CMUL, .type_name = "double", .doc = "optional factor to multiply with y (def: 1)" },
	{ .name = CADD, .type_name = "double", .doc = "optional offset to add to y after mul (def: 0)" },
	{ 0 },
};

ubx_proto_port_t math_ports[] = {
	{ .name = "x", .in_type_name = "double", .out_data_len = 1, .doc = "math input"  },
	{ .name = "y", .out_type_name = "double", .out_data_len = 1, .doc = "math output"  },
	{ 0 },
};

struct math_info {

	const struct mathfunc *func;
	const double *mul;
	const double *add;

	ubx_port_t *p_x;
	ubx_port_t *p_y;

	long data_len;
};

int parse_func(ubx_block_t *b, struct math_info *inf)
{
	long len;
	const char *func;

	len = cfg_getptr_char(b, CFUNC, &func);
	assert(len > 0);

	for (unsigned int i=0; i<ARRAY_SIZE(functions); i++) {
		if (strncasecmp(func, functions[i].name, MATHFUNC_MAXLEN) == 0) {
			ubx_debug(b, "found math function %s", func);
			inf->func = &functions[i];
			return 0;
		}
	}
	ubx_err(b, "unsupported math function %s", func);
	return -1;
}

int math_init(ubx_block_t *b)
{
	int ret = -1;
	long len;
	const long *data_len;
	struct math_info *inf;

	inf = calloc(1, sizeof(struct math_info));

	if (inf == NULL) {
		ubx_err(b, "math: failed to alloc memory");
		return EOUTOFMEM;
	}

	b->private_data = inf;

	inf->p_x = ubx_port_get(b, "x");
	inf->p_y = ubx_port_get(b, "y");

	/* handle data_len conf */
	len = cfg_getptr_long(b, "data_len", &data_len);
	assert(len>=0);

	inf->data_len = (len > 0) ? *data_len : 1;

	/* resize ports */
	if (ubx_outport_resize(inf->p_x, inf->data_len) ||
	    ubx_inport_resize(inf->p_y, inf->data_len) != 0)
		return -1;

	ret = parse_func(b, inf);

	/* mul */
	len = cfg_getptr_double(b, CMUL, &inf->mul);
	assert(len>=0);

	if (len != 0 && len != inf->data_len) {
		ubx_err(b, "EINVALID_CONFIG_LEN of %s: expected %lu, got %lu",
			CMUL, inf->data_len, len);
		return EINVALID_CONFIG_LEN;
	}

	/* add */
	len = cfg_getptr_double(b, CADD, &inf->add);
	assert(len>=0);

	if (len != 0 && len != inf->data_len) {
		ubx_err(b, "EINVALID_CONFIG_LEN of %s: expected %lu, got %lu",
			CADD, inf->data_len, len);
		return EINVALID_CONFIG_LEN;
	}

	if (ret)
		return -1;

	return 0;
}

/* cleanup */
void math_cleanup(ubx_block_t *b)
{
	free(b->private_data);
}

/* step */
void math_step(ubx_block_t *b)
{
	long len;
	struct math_info *inf = (struct math_info *)b->private_data;

	double data[inf->data_len];

	len = read_double_array(inf->p_x, data, inf->data_len);

	if (len < 0) {
		ubx_err(b, "error reading port x: %lu", len);
		return;
	} else if (len == 0) {
		ubx_notice(b, "unexpected NO_DATA on port x");
		return;
	} else if (len != inf->data_len) {
		ubx_err(b, "EINVALID_DATA_LEN: %lu", len);
		return;
	}

	for (long i=0; i<inf->data_len; i++)
		data[i] = inf->func->f(data[i]);

	if (inf->mul){
		for (long i=0; i<inf->data_len; i++)
			data[i] *= inf->mul[i];
	}

	if (inf->add){
		for (long i=0; i<inf->data_len; i++)
			data[i] += inf->add[i];
	}

	write_double_array(inf->p_y, data, inf->data_len);
}

/* put everything together */
ubx_proto_block_t math_block = {
	.name = "math_double",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = math_meta,
	.configs = math_config,
	.ports = math_ports,

	.init = math_init,
	.cleanup = math_cleanup,
	.step = math_step,
};


int math_mod_init(ubx_node_info_t *ni)
{
	return ubx_block_register(ni, &math_block);
}

void math_mod_cleanup(ubx_node_info_t *ni)
{
	ubx_block_unregister(ni, "math_double");
}

UBX_MODULE_INIT(math_mod_init)
UBX_MODULE_CLEANUP(math_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
