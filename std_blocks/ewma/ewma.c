/*
 * A generic exponentially-weighted moving-average (EWMA) block.
 *
 * First-order exponential smoothing: y += alpha * (x - y), with the
 * first sample initializing y = x. The numeric type is configurable at
 * runtime via the 'type' config; input and output share that type, so
 * the block acts as a drop-in signal filter. With 'data_len' > 1 the
 * ports are vectors and an independent average is kept per element
 * (channel).
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ubx.h"

#define TYPE		"type"
#define ALPHA		"alpha"
#define DATA_LEN	"data_len"
#define PIN		"in"
#define POUT		"out"

char ewma_meta[] =
	"{ doc='exponentially-weighted moving-average (EWMA) filter', realtime=true }";

ubx_proto_config_t ewma_config[] = {
	{ .name = TYPE, .type_name = "char", .min = 1, .doc = "ubx numeric type name of the signal" },
	{ .name = ALPHA, .type_name = "double", .min = 1, .max = 1, .doc = "smoothing factor (0 < alpha <= 1); smaller smooths more" },
	{ .name = DATA_LEN, .type_name = "long", .min = 0, .max = 1, .doc = "vector length; averaged per element (default 1)" },
	/* if a 'loglevel' config is defined, it will automatically
	 * affect the block loglevel. If unset the global loglevel is used */
	{ .name = "loglevel", .type_name = "int" },
	{ 0 },
};

/* Note: the 'in' and 'out' ports are added at runtime in init with the
 * configured type. */
ubx_proto_port_t ewma_ports[] = {
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

struct ewma_info {
	to_double_fn to_double;
	from_double_fn from_double;
	long elem_size;		/* size of one element [bytes] */

	double alpha;		/* smoothing factor */
	long data_len;		/* number of channels (vector length) */
	double *y;		/* per-channel filter state [data_len] */
	int initialized;	/* 0 until the first sample seeded y */

	ubx_data_t *sample;	/* read/write buffer */
	ubx_port_t *p_in;
	ubx_port_t *p_out;
};

static int ewma_init(ubx_block_t *b)
{
	int ret = EINVALID_CONFIG;
	long len;
	const char *type_name;
	const double *alpha;
	const long *data_len;
	const struct numtype *conv;
	struct ewma_info *inf;

	b->private_data = calloc(1, sizeof(struct ewma_info));

	if (b->private_data == NULL) {
		ubx_err(b, "EOUTOFMEM: failed to alloc ewma_info");
		return EOUTOFMEM;
	}

	inf = (struct ewma_info *)b->private_data;

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

	/* alpha */
	len = cfg_getptr_double(b, ALPHA, &alpha);
	assert(len >= 0);

	if (len == 0) {
		ubx_err(b, "EINVALID_CONFIG: mandatory config %s unset", ALPHA);
		goto out_free;
	}

	if (!(*alpha > 0 && *alpha <= 1)) {
		ubx_err(b, "EINVALID_CONFIG: %s must be in (0, 1] (got %g)",
			ALPHA, *alpha);
		goto out_free;
	}

	inf->alpha = *alpha;

	/* data_len, default 1 */
	len = cfg_getptr_long(b, DATA_LEN, &data_len);
	assert(len >= 0);
	inf->data_len = (len > 0) ? *data_len : 1;

	if (inf->data_len < 1) {
		ubx_err(b, "EINVALID_CONFIG: %s must be >= 1", DATA_LEN);
		goto out_free;
	}

	/* add the runtime-typed ports */
	ret = ubx_inport_add(b, PIN, "input signal", 0, type_name, inf->data_len);

	if (ret != 0)
		goto out_free;

	ret = ubx_outport_add(b, POUT, "EWMA output", 0, type_name, inf->data_len);

	if (ret != 0)
		goto out_port_in;

	/* allocate the filter state and the read/write buffer */
	inf->y = calloc(inf->data_len, sizeof(double));
	inf->sample = ubx_data_alloc(b->nd, type_name, inf->data_len);

	if (inf->y == NULL || inf->sample == NULL) {
		ubx_err(b, "EOUTOFMEM: failed to alloc buffers");
		ret = EOUTOFMEM;
		goto out_buffers;
	}

	inf->initialized = 0;

	return 0;

out_buffers:
	free(inf->y);
	ubx_data_free(inf->sample);
	ubx_port_rm(b, POUT);
out_port_in:
	ubx_port_rm(b, PIN);
out_free:
	free(b->private_data);
	b->private_data = NULL;
	return ret;
}

static int ewma_start(ubx_block_t *b)
{
	struct ewma_info *inf = (struct ewma_info *)b->private_data;

	inf->p_in = ubx_port_get(b, PIN);
	inf->p_out = ubx_port_get(b, POUT);
	assert(inf->p_in != NULL && inf->p_out != NULL);

	/* Note: the filter state is deliberately not reset here - it
	 * persists across stop/start and is only cleared on (re-)init. */
	return 0;
}

void ewma_step(ubx_block_t *b)
{
	long len;
	struct ewma_info *inf = (struct ewma_info *)b->private_data;

	len = __port_read(inf->p_in, inf->sample);

	if (len <= 0)
		return;		/* NODATA */

	if (len != inf->data_len) {
		ubx_err(b, "in value has wrong length (got %ld, expected %ld)",
			len, inf->data_len);
		return;
	}

	/* y += alpha * (x - y); the first sample seeds y = x */
	for (long ch = 0; ch < inf->data_len; ch++) {
		void *elem = (char *)inf->sample->data + ch * inf->elem_size;
		double x = inf->to_double(elem);

		if (!inf->initialized)
			inf->y[ch] = x;
		else
			inf->y[ch] += inf->alpha * (x - inf->y[ch]);

		inf->from_double(elem, inf->y[ch]);
	}

	inf->initialized = 1;

	__port_write(inf->p_out, inf->sample);
}

void ewma_cleanup(ubx_block_t *b)
{
	struct ewma_info *inf = (struct ewma_info *)b->private_data;

	free(inf->y);
	ubx_data_free(inf->sample);
	ubx_port_rm(b, POUT);
	ubx_port_rm(b, PIN);
	free(b->private_data);
	b->private_data = NULL;
}

ubx_proto_block_t ewma_comp = {
	.name = "ubx/ewma",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = ewma_meta,
	.configs = ewma_config,
	.ports = ewma_ports,

	.init = ewma_init,
	.start = ewma_start,
	.step = ewma_step,
	.cleanup = ewma_cleanup,
};

static int ewma_mod_init(ubx_node_t *nd)
{
	return ubx_block_register(nd, &ewma_comp);
}

static void ewma_mod_cleanup(ubx_node_t *nd)
{
	ubx_block_unregister(nd, "ubx/ewma");
}

UBX_MODULE_INIT(ewma_mod_init)
UBX_MODULE_CLEANUP(ewma_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
