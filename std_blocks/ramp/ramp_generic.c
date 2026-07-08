/*
 * A generic, runtime-typed ramp generator block.
 *
 * Each step the current value is written to 'out' and then incremented
 * by 'slope'. The numeric type is configurable at runtime via the
 * 'type' config; with 'data_len' > 1 the output is a vector with an
 * independent ramp per element.
 *
 * Unlike the filter blocks (movavg, ewma, ...) that compute in double
 * internally, the ramp accumulates in the *native* type: a per-type
 * increment kernel is selected once at init, so integer ramps count
 * exactly over the full type range (no 2^53 double mantissa limit) and
 * unsigned ramps wrap as expected. The 'start'/'slope' configs are
 * given as double and converted (range-checked, rounded to nearest)
 * once at init.
 *
 * This block supersedes the compile-time typed ubx/ramp_<type>
 * variants, which are kept for backwards compatibility.
 */

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ubx.h"

#define TYPE		"type"
#define DATA_LEN	"data_len"
#define START		"start"
#define SLOPE		"slope"
#define POUT		"out"

char rampg_meta[] =
	"{ doc='runtime-typed ramp generator', realtime=true }";

ubx_proto_config_t rampg_config[] = {
	{ .name = TYPE, .type_name = "char", .min = 1, .doc = "ubx numeric type name of the output" },
	{ .name = DATA_LEN, .type_name = "long", .min = 0, .max = 1, .doc = "vector length (default 1)" },
	{ .name = START, .type_name = "double", .doc = "start value: scalar or per-element [data_len] (default 0)" },
	{ .name = SLOPE, .type_name = "double", .min = 1, .doc = "increment per step: scalar or per-element [data_len]" },
	/* if a 'loglevel' config is defined, it will automatically
	 * affect the block loglevel. If unset the global loglevel is used */
	{ .name = "loglevel", .type_name = "int" },
	{ 0 },
};

/* Note: the 'out' port is added at runtime in init with the configured
 * type. */
ubx_proto_port_t rampg_ports[] = {
	{ 0 },
};

/*
 * per-type kernels: 'add' increments natively (selected once at init,
 * used every step), 'from_double' converts a config value at init
 * (range-checked, integers rounded to nearest; returns -1 if out of
 * range).
 */
typedef void (*add_fn)(void *cur, const void *slope, long n);
typedef int (*from_double_fn)(void *dst, double v);

#define DEF_RAMP_ADD(SFX, T)						\
	static void add_##SFX(void *cur, const void *slope, long n)	\
	{								\
		T *c = (T *)cur;					\
		const T *s = (const T *)slope;				\
									\
		for (long i = 0; i < n; i++)				\
			c[i] += s[i];					\
	}

#define DEF_RAMP_INT(SFX, T, MIN, MAX)					\
	DEF_RAMP_ADD(SFX, T)						\
	static int fd_##SFX(void *dst, double v)			\
	{								\
		if (!(v >= (double)(MIN) && v <= (double)(MAX)))	\
			return -1;					\
		*(T *)dst = (T)nearbyint(v);				\
		return 0;						\
	}

#define DEF_RAMP_FLT(SFX, T)						\
	DEF_RAMP_ADD(SFX, T)						\
	static int fd_##SFX(void *dst, double v)			\
	{								\
		*(T *)dst = (T)v;					\
		return 0;						\
	}

DEF_RAMP_INT(i8,  int8_t,   INT8_MIN,  INT8_MAX)
DEF_RAMP_INT(i16, int16_t,  INT16_MIN, INT16_MAX)
DEF_RAMP_INT(i32, int32_t,  INT32_MIN, INT32_MAX)
DEF_RAMP_INT(i64, int64_t,  INT64_MIN, INT64_MAX)
DEF_RAMP_INT(u8,  uint8_t,  0,         UINT8_MAX)
DEF_RAMP_INT(u16, uint16_t, 0,         UINT16_MAX)
DEF_RAMP_INT(u32, uint32_t, 0,         UINT32_MAX)
DEF_RAMP_INT(u64, uint64_t, 0,         UINT64_MAX)
DEF_RAMP_FLT(flt, float)
DEF_RAMP_FLT(dbl, double)

struct numtype {
	const char *name;
	long size;
	add_fn add;
	from_double_fn from_double;
};

static const struct numtype numtypes[] = {
	{ "int8_t",	sizeof(int8_t),		add_i8,		fd_i8 },
	{ "int16_t",	sizeof(int16_t),	add_i16,	fd_i16 },
	{ "int32_t",	sizeof(int32_t),	add_i32,	fd_i32 },
	{ "int64_t",	sizeof(int64_t),	add_i64,	fd_i64 },
	{ "uint8_t",	sizeof(uint8_t),	add_u8,		fd_u8 },
	{ "uint16_t",	sizeof(uint16_t),	add_u16,	fd_u16 },
	{ "uint32_t",	sizeof(uint32_t),	add_u32,	fd_u32 },
	{ "uint64_t",	sizeof(uint64_t),	add_u64,	fd_u64 },
	{ "float",	sizeof(float),		add_flt,	fd_flt },
	{ "double",	sizeof(double),		add_dbl,	fd_dbl },
};

static const struct numtype *lookup_conv(const char *name)
{
	for (unsigned int i = 0; i < ARRAY_SIZE(numtypes); i++)
		if (strcmp(numtypes[i].name, name) == 0)
			return &numtypes[i];
	return NULL;
}

struct rampg_info {
	const struct numtype *nt;
	long data_len;		/* number of elements (vector length) */

	void *start;		/* native start values [data_len] */
	void *slope;		/* native slope values [data_len] */
	ubx_data_t *cur;	/* current value / write buffer [data_len] */

	ubx_port_t *p_out;
};

/*
 * load a double config into the native array dst[data_len],
 * broadcasting a scalar. Unset is OK (dst stays zeroed) unless
 * mandatory.
 */
static int load_cfg(ubx_block_t *b, const char *name, const struct numtype *nt,
		    void *dst, long data_len, int mandatory)
{
	const double *v;
	long len = cfg_getptr_double(b, name, &v);

	assert(len >= 0);

	if (len == 0) {
		if (mandatory) {
			ubx_err(b, "EINVALID_CONFIG: mandatory config %s unset", name);
			return -1;
		}
		return 0;
	}

	if (len != 1 && len != data_len) {
		ubx_err(b, "EINVALID_CONFIG_LEN: %s must have 1 or %ld elements (got %ld)",
			name, data_len, len);
		return -1;
	}

	for (long i = 0; i < data_len; i++) {
		double x = (len == 1) ? v[0] : v[i];

		if (nt->from_double((char *)dst + i * nt->size, x) != 0) {
			ubx_err(b, "EINVALID_CONFIG: %s[%ld] (%g) out of range for %s",
				name, i, x, nt->name);
			return -1;
		}
	}

	return 0;
}

static int rampg_init(ubx_block_t *b)
{
	int ret = EINVALID_CONFIG;
	long len;
	const char *type_name;
	const long *data_len;
	struct rampg_info *inf;

	b->private_data = calloc(1, sizeof(struct rampg_info));

	if (b->private_data == NULL) {
		ubx_err(b, "EOUTOFMEM: failed to alloc rampg_info");
		return EOUTOFMEM;
	}

	inf = (struct rampg_info *)b->private_data;

	/* type */
	len = cfg_getptr_char(b, TYPE, &type_name);
	assert(len >= 0);

	if (len == 0) {
		ubx_err(b, "EINVALID_CONFIG: mandatory config %s unset", TYPE);
		goto out_free;
	}

	if (ubx_type_get(b->nd, type_name) == NULL) {
		ubx_err(b, "unknown type %s", type_name);
		goto out_free;
	}

	inf->nt = lookup_conv(type_name);

	if (inf->nt == NULL) {
		ubx_err(b, "EINVALID_CONFIG: %s '%s' is not a supported numeric type",
			TYPE, type_name);
		goto out_free;
	}

	/* data_len, default 1 */
	len = cfg_getptr_long(b, DATA_LEN, &data_len);
	assert(len >= 0);
	inf->data_len = (len > 0) ? *data_len : 1;

	if (inf->data_len < 1) {
		ubx_err(b, "EINVALID_CONFIG: %s must be >= 1", DATA_LEN);
		goto out_free;
	}

	/* allocate the native start/slope arrays and the write buffer */
	inf->start = calloc(inf->data_len, inf->nt->size);
	inf->slope = calloc(inf->data_len, inf->nt->size);
	inf->cur = ubx_data_alloc(b->nd, type_name, inf->data_len);

	if (inf->start == NULL || inf->slope == NULL || inf->cur == NULL) {
		ubx_err(b, "EOUTOFMEM: failed to alloc buffers");
		ret = EOUTOFMEM;
		goto out_buffers;
	}

	/* convert the double configs to the native type (init-time only) */
	if (load_cfg(b, START, inf->nt, inf->start, inf->data_len, 0) != 0 ||
	    load_cfg(b, SLOPE, inf->nt, inf->slope, inf->data_len, 1) != 0)
		goto out_buffers;

	/* add the runtime-typed output port */
	ret = ubx_outport_add(b, POUT, "ramp output", 0, type_name, inf->data_len);

	if (ret != 0)
		goto out_buffers;

	return 0;

out_buffers:
	free(inf->start);
	free(inf->slope);
	ubx_data_free(inf->cur);
out_free:
	free(b->private_data);
	b->private_data = NULL;
	return ret;
}

static int rampg_start(ubx_block_t *b)
{
	struct rampg_info *inf = (struct rampg_info *)b->private_data;

	inf->p_out = ubx_port_get(b, POUT);
	assert(inf->p_out != NULL);

	/* (re-)initialize cur to start on every activation */
	memcpy(inf->cur->data, inf->start, inf->data_len * inf->nt->size);

	return 0;
}

void rampg_step(ubx_block_t *b)
{
	struct rampg_info *inf = (struct rampg_info *)b->private_data;

	__port_write(inf->p_out, inf->cur);
	inf->nt->add(inf->cur->data, inf->slope, inf->data_len);
}

void rampg_cleanup(ubx_block_t *b)
{
	struct rampg_info *inf = (struct rampg_info *)b->private_data;

	free(inf->start);
	free(inf->slope);
	ubx_data_free(inf->cur);
	ubx_port_rm(b, POUT);
	free(b->private_data);
	b->private_data = NULL;
}

ubx_proto_block_t rampg_comp = {
	.name = "ubx/ramp",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = rampg_meta,
	.configs = rampg_config,
	.ports = rampg_ports,

	.init = rampg_init,
	.start = rampg_start,
	.step = rampg_step,
	.cleanup = rampg_cleanup,
};

static int rampg_mod_init(ubx_node_t *nd)
{
	return ubx_block_register(nd, &rampg_comp);
}

static void rampg_mod_cleanup(ubx_node_t *nd)
{
	ubx_block_unregister(nd, "ubx/ramp");
}

UBX_MODULE_INIT(rampg_mod_init)
UBX_MODULE_CLEANUP(rampg_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
