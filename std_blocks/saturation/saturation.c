/*
 * A generic saturation (clamping) block.
 *
 * Clamps the values received on the 'in' port element-wise between
 * 'lower_limits' and 'upper_limits' and emits the result on the 'out'
 * port. The numeric type is configurable at runtime via the 'type'
 * config; input and output share that type, so the block is a drop-in
 * signal filter. With 'data_len' > 1 the ports are vectors; the limits
 * are then either per-element (length data_len) or scalar (length 1,
 * applied to all elements). Unclamped values pass through unmodified.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ubx.h"

#define TYPE		"type"
#define DATA_LEN	"data_len"
#define LOWER_LIMITS	"lower_limits"
#define UPPER_LIMITS	"upper_limits"
#define PIN		"in"
#define POUT		"out"

char sat_meta[] =
	"{ doc='element-wise saturation (clamping) block', realtime=true }";

ubx_proto_config_t sat_config[] = {
	{ .name = TYPE, .type_name = "char", .min = 1, .doc = "ubx numeric type name of the signal" },
	{ .name = DATA_LEN, .type_name = "long", .min = 0, .max = 1, .doc = "vector length (default 1)" },
	{ .name = LOWER_LIMITS, .type_name = "double", .min = 1, .doc = "lower bounds: scalar or per-element [data_len]" },
	{ .name = UPPER_LIMITS, .type_name = "double", .min = 1, .doc = "upper bounds: scalar or per-element [data_len]" },
	/* if a 'loglevel' config is defined, it will automatically
	 * affect the block loglevel. If unset the global loglevel is used */
	{ .name = "loglevel", .type_name = "int" },
	{ 0 },
};

/* Note: the 'in' and 'out' ports are added at runtime in init with the
 * configured type. */
ubx_proto_port_t sat_ports[] = {
	{ 0 },
};

/* runtime conversions between a numeric C type and double */
typedef double (*to_double_fn)(const void *);
typedef void (*from_double_fn)(void *, double);

static double td_i32(const void *p)    { return (double)*(const int32_t *)p; }
static double td_i64(const void *p)    { return (double)*(const int64_t *)p; }
static double td_u32(const void *p)    { return (double)*(const uint32_t *)p; }
static double td_u64(const void *p)    { return (double)*(const uint64_t *)p; }
static double td_float(const void *p)  { return (double)*(const float *)p; }
static double td_double(const void *p) { return *(const double *)p; }

/* integer stores round to nearest; floating-point stores cast directly */
static void fd_i32(void *p, double v)    { *(int32_t *)p = (int32_t)llround(v); }
static void fd_i64(void *p, double v)    { *(int64_t *)p = (int64_t)llround(v); }
static void fd_u32(void *p, double v)    { *(uint32_t *)p = (uint32_t)llround(v); }
static void fd_u64(void *p, double v)    { *(uint64_t *)p = (uint64_t)llround(v); }
static void fd_float(void *p, double v)  { *(float *)p = (float)v; }
static void fd_double(void *p, double v) { *(double *)p = v; }

struct numtype {
	const char *name;
	long size;
	to_double_fn to_double;
	from_double_fn from_double;
};

static const struct numtype numtypes[] = {
	{ "int32_t",	sizeof(int32_t),	td_i32,		fd_i32 },
	{ "int64_t",	sizeof(int64_t),	td_i64,		fd_i64 },
	{ "uint32_t",	sizeof(uint32_t),	td_u32,		fd_u32 },
	{ "uint64_t",	sizeof(uint64_t),	td_u64,		fd_u64 },
	{ "float",	sizeof(float),		td_float,	fd_float },
	{ "double",	sizeof(double),		td_double,	fd_double },
};

static const struct numtype *lookup_conv(const char *name)
{
	for (unsigned int i = 0; i < ARRAY_SIZE(numtypes); i++)
		if (strcmp(numtypes[i].name, name) == 0)
			return &numtypes[i];
	return NULL;
}

struct sat_info {
	to_double_fn to_double;
	from_double_fn from_double;
	long elem_size;		/* size of one element [bytes] */

	long data_len;		/* number of elements (vector length) */
	double *lower;		/* expanded lower bounds [data_len] */
	double *upper;		/* expanded upper bounds [data_len] */

	ubx_data_t *sample;	/* read/clamp/write buffer */
	ubx_port_t *p_in;
	ubx_port_t *p_out;
};

/* load a limits config into dst[data_len], broadcasting a scalar */
static int load_limits(ubx_block_t *b, const char *name, double *dst, long data_len)
{
	const double *v;
	long len = cfg_getptr_double(b, name, &v);

	assert(len >= 0);

	if (len == 1) {
		for (long i = 0; i < data_len; i++)
			dst[i] = v[0];
		return 0;
	}

	if (len == data_len) {
		memcpy(dst, v, data_len * sizeof(double));
		return 0;
	}

	ubx_err(b, "EINVALID_CONFIG_LEN: %s must have 1 or %ld elements (got %ld)",
		name, data_len, len);
	return -1;
}

static int sat_init(ubx_block_t *b)
{
	int ret = EINVALID_CONFIG;
	long len;
	const char *type_name;
	const long *data_len;
	const struct numtype *conv;
	struct sat_info *inf;

	b->private_data = calloc(1, sizeof(struct sat_info));

	if (b->private_data == NULL) {
		ubx_err(b, "EOUTOFMEM: failed to alloc sat_info");
		return EOUTOFMEM;
	}

	inf = (struct sat_info *)b->private_data;

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

	conv = lookup_conv(type_name);

	if (conv == NULL) {
		ubx_err(b, "EINVALID_CONFIG: %s '%s' is not a supported numeric type",
			TYPE, type_name);
		goto out_free;
	}

	inf->to_double = conv->to_double;
	inf->from_double = conv->from_double;
	inf->elem_size = conv->size;

	/* data_len, default 1 */
	len = cfg_getptr_long(b, DATA_LEN, &data_len);
	assert(len >= 0);
	inf->data_len = (len > 0) ? *data_len : 1;

	if (inf->data_len < 1) {
		ubx_err(b, "EINVALID_CONFIG: %s must be >= 1", DATA_LEN);
		goto out_free;
	}

	/* limits */
	inf->lower = calloc(inf->data_len, sizeof(double));
	inf->upper = calloc(inf->data_len, sizeof(double));

	if (inf->lower == NULL || inf->upper == NULL) {
		ubx_err(b, "EOUTOFMEM: failed to alloc limits");
		ret = EOUTOFMEM;
		goto out_limits;
	}

	if (load_limits(b, LOWER_LIMITS, inf->lower, inf->data_len) != 0 ||
	    load_limits(b, UPPER_LIMITS, inf->upper, inf->data_len) != 0)
		goto out_limits;

	for (long i = 0; i < inf->data_len; i++) {
		if (inf->lower[i] > inf->upper[i]) {
			ubx_err(b, "EINVALID_CONFIG: %s[%ld] (%g) > %s[%ld] (%g)",
				LOWER_LIMITS, i, inf->lower[i],
				UPPER_LIMITS, i, inf->upper[i]);
			goto out_limits;
		}
	}

	/* add the runtime-typed ports */
	ret = ubx_inport_add(b, PIN, "input signal", 0, type_name, inf->data_len);

	if (ret != 0)
		goto out_limits;

	ret = ubx_outport_add(b, POUT, "saturated output", 0, type_name, inf->data_len);

	if (ret != 0)
		goto out_port_in;

	/* allocate the read/clamp/write buffer */
	inf->sample = ubx_data_alloc(b->nd, type_name, inf->data_len);

	if (inf->sample == NULL) {
		ubx_err(b, "EOUTOFMEM: failed to alloc sample buffer");
		ret = EOUTOFMEM;
		goto out_port_out;
	}

	return 0;

out_port_out:
	ubx_port_rm(b, POUT);
out_port_in:
	ubx_port_rm(b, PIN);
out_limits:
	free(inf->lower);
	free(inf->upper);
out_free:
	free(b->private_data);
	b->private_data = NULL;
	return ret;
}

static int sat_start(ubx_block_t *b)
{
	struct sat_info *inf = (struct sat_info *)b->private_data;

	inf->p_in = ubx_port_get(b, PIN);
	inf->p_out = ubx_port_get(b, POUT);
	assert(inf->p_in != NULL && inf->p_out != NULL);

	return 0;
}

void sat_step(ubx_block_t *b)
{
	long len;
	struct sat_info *inf = (struct sat_info *)b->private_data;

	len = __port_read(inf->p_in, inf->sample);

	if (len <= 0)
		return;		/* NODATA */

	if (len != inf->data_len) {
		ubx_err(b, "in value has wrong length (got %ld, expected %ld)",
			len, inf->data_len);
		return;
	}

	/* clamp in place: unclamped values pass through unmodified */
	for (long i = 0; i < inf->data_len; i++) {
		void *elem = (char *)inf->sample->data + i * inf->elem_size;
		double x = inf->to_double(elem);

		if (x > inf->upper[i])
			inf->from_double(elem, inf->upper[i]);
		else if (x < inf->lower[i])
			inf->from_double(elem, inf->lower[i]);
	}

	__port_write(inf->p_out, inf->sample);
}

void sat_cleanup(ubx_block_t *b)
{
	struct sat_info *inf = (struct sat_info *)b->private_data;

	ubx_data_free(inf->sample);
	free(inf->lower);
	free(inf->upper);
	ubx_port_rm(b, POUT);
	ubx_port_rm(b, PIN);
	free(b->private_data);
	b->private_data = NULL;
}

ubx_proto_block_t sat_comp = {
	.name = "ubx/saturation",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = sat_meta,
	.configs = sat_config,
	.ports = sat_ports,

	.init = sat_init,
	.start = sat_start,
	.step = sat_step,
	.cleanup = sat_cleanup,
};

static int sat_mod_init(ubx_node_t *nd)
{
	return ubx_block_register(nd, &sat_comp);
}

static void sat_mod_cleanup(ubx_node_t *nd)
{
	ubx_block_unregister(nd, "ubx/saturation");
}

UBX_MODULE_INIT(sat_mod_init)
UBX_MODULE_CLEANUP(sat_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
