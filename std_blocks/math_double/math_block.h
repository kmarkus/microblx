/*
 * math function block - parameterized by MATH_T (float or double)
 */

#ifndef MATH_T
# error "MATH_T undefined"
#endif

#undef UBX_DEBUG

#include <string.h>
#include <math.h>
#include "ubx.h"

UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)

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
	{ .name = CMUL, .type_name = QUOTE(MATH_T), .doc = "optional factor to multiply with y (def: 1)" },
	{ .name = CADD, .type_name = QUOTE(MATH_T), .doc = "optional offset to add to y after mul (def: 0)" },
	{ 0 },
};

ubx_proto_port_t math_ports[] = {
	{ .name = "x", .in_type_name = QUOTE(MATH_T), .out_data_len = 1, .doc = "math input"  },
	{ .name = "y", .out_type_name = QUOTE(MATH_T), .out_data_len = 1, .doc = "math output"  },
	{ 0 },
};

struct math_info {
	const struct mathfunc *func;
	const MATH_T *mul;
	const MATH_T *add;

	ubx_port_t *p_x;
	ubx_port_t *p_y;

	long data_len;
};

/* helpers to expand type-specific API calls from MATH_T */
#define _MATH_CONCAT_IMPL(a, b)      a##b
#define _MATH_CONCAT(a, b)           _MATH_CONCAT_IMPL(a, b)
#define _MATH_CONCAT3_IMPL(a, b, c)  a##b##c
#define _MATH_CONCAT3(a, b, c)       _MATH_CONCAT3_IMPL(a, b, c)
#define MATH_CFG_GETPTR(b, name, ptr)     _MATH_CONCAT(cfg_getptr_, MATH_T)(b, name, ptr)
#define MATH_READ_ARRAY(port, data, len)  _MATH_CONCAT3(read_, MATH_T, _array)(port, data, len)
#define MATH_WRITE_ARRAY(port, data, len) _MATH_CONCAT3(write_, MATH_T, _array)(port, data, len)

/* forward declarations */
int math_init(ubx_block_t *b);
void math_cleanup(ubx_block_t *b);
void math_step(ubx_block_t *b);

ubx_proto_block_t math_block = {
	.name = "ubx/math_" QUOTE(MATH_T),
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = math_meta,
	.configs = math_config,
	.ports = math_ports,

	.init = math_init,
	.cleanup = math_cleanup,
	.step = math_step,
};

int math_mod_init(ubx_node_t *nd)
{
	return ubx_block_register(nd, &math_block);
}

void math_mod_cleanup(ubx_node_t *nd)
{
	ubx_block_unregister(nd, "ubx/math_" QUOTE(MATH_T));
}

UBX_MODULE_INIT(math_mod_init)
UBX_MODULE_CLEANUP(math_mod_cleanup)
