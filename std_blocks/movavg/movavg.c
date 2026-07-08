/*
 * A generic fixed-window sliding filter block.
 *
 * Maintains a ring buffer of the last 'window' values received on the
 * 'in' port and emits an aggregate ('mode': mean (default), median, min
 * or max) on the 'out' port. The numeric type is configurable at
 * runtime via the 'type' config; input and output share that type, so
 * the block acts as a drop-in signal filter. With 'data_len' > 1 the
 * ports are vectors and an independent filter is kept per element
 * (channel). Before the window has filled, the aggregate is computed
 * over the samples seen so far.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ubx.h"

#define TYPE		"type"
#define WINDOW		"window"
#define DATA_LEN	"data_len"
#define MODE		"mode"
#define PIN		"in"
#define POUT		"out"

char movavg_meta[] =
	"{ doc='fixed-window sliding filter (mean/median/min/max)', realtime=true }";

ubx_proto_config_t movavg_config[] = {
	{ .name = TYPE, .type_name = "char", .min = 1, .doc = "ubx numeric type name of the signal" },
	{ .name = WINDOW, .type_name = "long", .min = 1, .max = 1, .doc = "number of samples in the window" },
	{ .name = DATA_LEN, .type_name = "long", .min = 0, .max = 1, .doc = "vector length; filtered per element (default 1)" },
	{ .name = MODE, .type_name = "char", .doc = "aggregate: 'mean' (default), 'median', 'min' or 'max'" },
	/* if a 'loglevel' config is defined, it will automatically
	 * affect the block loglevel. If unset the global loglevel is used */
	{ .name = "loglevel", .type_name = "int" },
	{ 0 },
};

/* Note: the 'in' and 'out' ports are added at runtime in init with the
 * configured type. */
ubx_proto_port_t movavg_ports[] = {
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

enum movavg_mode { MODE_MEAN, MODE_MEDIAN, MODE_MIN, MODE_MAX };

static const char *mode_names[] = {
	[MODE_MEAN] = "mean",
	[MODE_MEDIAN] = "median",
	[MODE_MIN] = "min",
	[MODE_MAX] = "max",
};

struct movavg_info {
	to_double_fn to_double;
	from_double_fn from_double;
	long elem_size;		/* size of one element [bytes] */

	enum movavg_mode mode;	/* aggregate to compute over the window */
	long window;		/* size of the sliding window */
	long data_len;		/* number of channels (vector length) */
	double *ring;		/* per-channel ring buffers [data_len * window] */
	double *scratch;	/* sort scratch for median [window], else NULL */
	long count;		/* number of valid samples (<= window) */
	long head;		/* index of the next slot to write */

	ubx_data_t *in_sample;	/* read buffer for the 'in' port */
	ubx_data_t *out_sample;	/* write buffer for the 'out' port */
	ubx_port_t *p_in;
	ubx_port_t *p_out;
};

static void movavg_reset(struct movavg_info *inf)
{
	inf->count = 0;
	inf->head = 0;
}

static int movavg_init(ubx_block_t *b)
{
	int ret = EINVALID_CONFIG;
	long len;
	const char *type_name;
	const long *window;
	const long *data_len;
	const struct numtype *conv;
	struct movavg_info *inf;

	b->private_data = calloc(1, sizeof(struct movavg_info));

	if (b->private_data == NULL) {
		ubx_err(b, "EOUTOFMEM: failed to alloc movavg_info");
		return EOUTOFMEM;
	}

	inf = (struct movavg_info *)b->private_data;

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

	/* window */
	len = cfg_getptr_long(b, WINDOW, &window);
	assert(len >= 0);

	if (len == 0) {
		ubx_err(b, "EINVALID_CONFIG: mandatory config %s unset", WINDOW);
		goto out_free;
	}

	if (*window < 1) {
		ubx_err(b, "EINVALID_CONFIG: %s must be >= 1", WINDOW);
		goto out_free;
	}

	inf->window = *window;

	/* data_len, default 1 */
	len = cfg_getptr_long(b, DATA_LEN, &data_len);
	assert(len >= 0);
	inf->data_len = (len > 0) ? *data_len : 1;

	if (inf->data_len < 1) {
		ubx_err(b, "EINVALID_CONFIG: %s must be >= 1", DATA_LEN);
		goto out_free;
	}

	/* mode, default 'mean' */
	{
		const char *mode;
		unsigned int i;

		len = cfg_getptr_char(b, MODE, &mode);
		assert(len >= 0);

		inf->mode = MODE_MEAN;

		if (len > 0) {
			for (i = 0; i < ARRAY_SIZE(mode_names); i++) {
				if (strcmp(mode, mode_names[i]) == 0) {
					inf->mode = i;
					break;
				}
			}

			if (i == ARRAY_SIZE(mode_names)) {
				ubx_err(b, "EINVALID_CONFIG: unknown %s '%s'", MODE, mode);
				goto out_free;
			}
		}
	}

	/* add the runtime-typed ports */
	ret = ubx_inport_add(b, PIN, "input signal", 0, type_name, inf->data_len);

	if (ret != 0)
		goto out_free;

	ret = ubx_outport_add(b, POUT, "moving-average output", 0, type_name, inf->data_len);

	if (ret != 0)
		goto out_port_in;

	/* allocate the per-channel rings, the median sort scratch (if
	 * needed) and the read/write buffers */
	inf->ring = calloc(inf->window * inf->data_len, sizeof(double));
	inf->scratch = (inf->mode == MODE_MEDIAN) ?
		calloc(inf->window, sizeof(double)) : NULL;
	inf->in_sample = ubx_data_alloc(b->nd, type_name, inf->data_len);
	inf->out_sample = ubx_data_alloc(b->nd, type_name, inf->data_len);

	if (inf->ring == NULL || inf->in_sample == NULL || inf->out_sample == NULL ||
	    (inf->mode == MODE_MEDIAN && inf->scratch == NULL)) {
		ubx_err(b, "EOUTOFMEM: failed to alloc buffers");
		ret = EOUTOFMEM;
		goto out_buffers;
	}

	movavg_reset(inf);

	return 0;

out_buffers:
	free(inf->ring);
	free(inf->scratch);
	ubx_data_free(inf->in_sample);
	ubx_data_free(inf->out_sample);
	ubx_port_rm(b, POUT);
out_port_in:
	ubx_port_rm(b, PIN);
out_free:
	free(b->private_data);
	b->private_data = NULL;
	return ret;
}

static int movavg_start(ubx_block_t *b)
{
	struct movavg_info *inf = (struct movavg_info *)b->private_data;

	inf->p_in = ubx_port_get(b, PIN);
	inf->p_out = ubx_port_get(b, POUT);
	assert(inf->p_in != NULL && inf->p_out != NULL);

	/* Note: the window is deliberately not reset here - it persists
	 * across stop/start and is only cleared on (re-)init. */
	return 0;
}

/* compute the configured aggregate over vals[count] (count >= 1).
 * 'median' insertion-sorts into the preallocated scratch buffer. */
static double movavg_aggregate(struct movavg_info *inf, const double *vals, long count)
{
	double ret;

	switch (inf->mode) {
	case MODE_MEAN:
		ret = 0;
		for (long i = 0; i < count; i++)
			ret += vals[i];
		return ret / (double)count;

	case MODE_MEDIAN:
		for (long i = 0; i < count; i++) {
			long j = i;

			while (j > 0 && inf->scratch[j - 1] > vals[i]) {
				inf->scratch[j] = inf->scratch[j - 1];
				j--;
			}
			inf->scratch[j] = vals[i];
		}
		return (count % 2) ? inf->scratch[count / 2] :
			0.5 * (inf->scratch[count / 2 - 1] + inf->scratch[count / 2]);

	case MODE_MIN:
		ret = vals[0];
		for (long i = 1; i < count; i++)
			ret = (vals[i] < ret) ? vals[i] : ret;
		return ret;

	case MODE_MAX:
	default:
		ret = vals[0];
		for (long i = 1; i < count; i++)
			ret = (vals[i] > ret) ? vals[i] : ret;
		return ret;
	}
}

void movavg_step(ubx_block_t *b)
{
	long len;
	struct movavg_info *inf = (struct movavg_info *)b->private_data;

	len = __port_read(inf->p_in, inf->in_sample);

	if (len <= 0)
		return;		/* NODATA */

	if (len != inf->data_len) {
		ubx_err(b, "in value has wrong length (got %ld, expected %ld)",
			len, inf->data_len);
		return;
	}

	/* push each channel's value into its ring at the shared head */
	for (long ch = 0; ch < inf->data_len; ch++) {
		const void *elem = (const char *)inf->in_sample->data + ch * inf->elem_size;
		inf->ring[ch * inf->window + inf->head] = inf->to_double(elem);
	}

	inf->head = (inf->head + 1) % inf->window;

	if (inf->count < inf->window)
		inf->count++;

	/* per-channel aggregate over the valid samples */
	for (long ch = 0; ch < inf->data_len; ch++) {
		const double *chring = &inf->ring[ch * inf->window];
		void *elem = (char *)inf->out_sample->data + ch * inf->elem_size;

		inf->from_double(elem, movavg_aggregate(inf, chring, inf->count));
	}

	__port_write(inf->p_out, inf->out_sample);
}

void movavg_cleanup(ubx_block_t *b)
{
	struct movavg_info *inf = (struct movavg_info *)b->private_data;

	free(inf->ring);
	free(inf->scratch);
	ubx_data_free(inf->in_sample);
	ubx_data_free(inf->out_sample);
	ubx_port_rm(b, POUT);
	ubx_port_rm(b, PIN);
	free(b->private_data);
	b->private_data = NULL;
}

ubx_proto_block_t movavg_comp = {
	.name = "ubx/movavg",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = movavg_meta,
	.configs = movavg_config,
	.ports = movavg_ports,

	.init = movavg_init,
	.start = movavg_start,
	.step = movavg_step,
	.cleanup = movavg_cleanup,
};

static int movavg_mod_init(ubx_node_t *nd)
{
	return ubx_block_register(nd, &movavg_comp);
}

static void movavg_mod_cleanup(ubx_node_t *nd)
{
	ubx_block_unregister(nd, "ubx/movavg");
}

UBX_MODULE_INIT(movavg_mod_init)
UBX_MODULE_CLEANUP(movavg_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
