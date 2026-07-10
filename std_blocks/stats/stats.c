/*
 * A generic statistics computation block.
 *
 * Accumulates min/max/mean/stddev/cnt over the values received on the
 * 'in' port and emits them as 'struct ubx_stat' on the 'stats' port.
 * The numeric input type is configurable at runtime via the 'type'
 * config. With 'data_len' > 1 the 'in' port is a vector and statistics
 * are kept per element (channel): index i of the output describes the
 * i-th vector element across all steps. Mean and (population) standard
 * deviation are computed with Welford's online algorithm.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ubx.h"

#include "types/ubx_stat.h"
#include "types/ubx_stat.h.hexarr"

#define TYPE			"type"
#define DATA_LEN		"data_len"
#define STATS_OUTPUT_RATE	"stats_output_rate"
#define PIN			"in"
#define PSTATS			"stats"

ubx_type_t ubx_stat_type = def_struct_type(struct ubx_stat, &ubx_stat_h);

/* define write_ubx_stat()/read_ubx_stat() port accessors */
def_port_accessors(ubx_stat, struct ubx_stat)

char stats_meta[] =
	"{ doc='generic min/max/mean/stddev/cnt statistics block', realtime=true }";

ubx_proto_config_t stats_config[] = {
	{ .name = TYPE, .type_name = "char", .min = 1, .doc = "ubx numeric type name of the input signal" },
	{ .name = DATA_LEN, .type_name = "long", .min = 0, .max = 1, .doc = "vector length; stats are kept per element (default 1)" },
	{ .name = STATS_OUTPUT_RATE, .type_name = "double", .min = 0, .max = 1, .doc = "min seconds between stats port outputs (0: every step)" },
	/* if a 'loglevel' config is defined, it will automatically
	 * affect the block loglevel. If unset the global loglevel is used */
	{ .name = "loglevel", .type_name = "int" },
	{ 0 },
};

/* Note: the 'in' port is added at runtime in init with the configured
 * type. Only the fixed-type 'stats' output port is declared here. */
ubx_proto_port_t stats_ports[] = {
	{ .name = PSTATS, .out_type_name = "struct ubx_stat", .doc = "running statistics output" },
	{ 0 },
};

/* convert a value of a given numeric C type to double */
typedef double (*to_double_fn)(const void *);

static double c_i32(const void *p)    { return (double)*(const int32_t *)p; }
static double c_i64(const void *p)    { return (double)*(const int64_t *)p; }
static double c_u32(const void *p)    { return (double)*(const uint32_t *)p; }
static double c_u64(const void *p)    { return (double)*(const uint64_t *)p; }
static double c_float(const void *p)  { return (double)*(const float *)p; }
static double c_double(const void *p) { return *(const double *)p; }

struct numtype {
	const char *name;
	long size;
	to_double_fn fn;
};

static const struct numtype numtypes[] = {
	{ "int32_t",	sizeof(int32_t),	c_i32 },
	{ "int64_t",	sizeof(int64_t),	c_i64 },
	{ "uint32_t",	sizeof(uint32_t),	c_u32 },
	{ "uint64_t",	sizeof(uint64_t),	c_u64 },
	{ "float",	sizeof(float),		c_float },
	{ "double",	sizeof(double),		c_double },
};

static const struct numtype *lookup_conv(const char *name)
{
	for (unsigned int i = 0; i < ARRAY_SIZE(numtypes); i++)
		if (strcmp(numtypes[i].name, name) == 0)
			return &numtypes[i];
	return NULL;
}

/* per-channel Welford accumulator */
struct chan {
	double mean;
	double m2;		/* sum of squares of differences from mean */
	double min;
	double max;
};

struct stats_info {
	to_double_fn to_double;
	long elem_size;		/* size of one input element [bytes] */
	long data_len;		/* number of channels (vector length) */

	unsigned long cnt;	/* sample count (shared, all channels advance together) */
	struct chan *chan;	/* per-channel accumulators [data_len] */
	struct ubx_stat *out;	/* scratch output buffer [data_len] */

	/* output throttling */
	uint64_t output_rate_ns;
	uint64_t output_last_ns;

	ubx_data_t *sample;	/* read buffer for the 'in' port */
	ubx_port_t *p_in;
	ubx_port_t *p_stats;
};

static void stats_reset(struct stats_info *inf)
{
	inf->cnt = 0;

	for (long i = 0; i < inf->data_len; i++) {
		inf->chan[i].mean = 0;
		inf->chan[i].m2 = 0;
		inf->chan[i].min = INFINITY;
		inf->chan[i].max = -INFINITY;
	}

	inf->output_last_ns = 0;
}

static int stats_init(ubx_block_t *b)
{
	int ret = EINVALID_CONFIG;
	long len;
	const char *type_name;
	const long *data_len;
	const double *rate;
	const struct numtype *conv;
	struct stats_info *inf;

	b->private_data = calloc(1, sizeof(struct stats_info));

	if (b->private_data == NULL) {
		ubx_err(b, "EOUTOFMEM: failed to alloc stats_info");
		return EOUTOFMEM;
	}

	inf = (struct stats_info *)b->private_data;

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

	inf->to_double = conv->fn;
	inf->elem_size = conv->size;

	/* data_len, default 1 */
	len = cfg_getptr_long(b, DATA_LEN, &data_len);
	assert(len >= 0);
	inf->data_len = (len > 0) ? *data_len : 1;

	if (inf->data_len < 1) {
		ubx_err(b, "EINVALID_CONFIG: %s must be >= 1", DATA_LEN);
		goto out_free;
	}

	/* stats_output_rate */
	len = cfg_getptr_double(b, STATS_OUTPUT_RATE, &rate);
	assert(len >= 0);
	inf->output_rate_ns = (len > 0) ? (uint64_t)(*rate * NSEC_PER_SEC) : 0;

	/* add the runtime-typed input port and resize the stats port */
	ret = ubx_inport_add(b, PIN, "input signal to accumulate", 0, type_name, inf->data_len);

	if (ret != 0)
		goto out_free;

	inf->p_stats = ubx_port_get(b, PSTATS);
	assert(inf->p_stats != NULL);

	if (ubx_outport_resize(inf->p_stats, inf->data_len) != 0) {
		ret = EINVALID_PORT_LEN;
		goto out_port;
	}

	/* allocate the read buffer and per-channel state */
	inf->sample = ubx_data_alloc(b->nd, type_name, inf->data_len);
	inf->chan = calloc(inf->data_len, sizeof(struct chan));
	inf->out = calloc(inf->data_len, sizeof(struct ubx_stat));

	if (inf->sample == NULL || inf->chan == NULL || inf->out == NULL) {
		ubx_err(b, "EOUTOFMEM: failed to alloc stats buffers");
		ret = EOUTOFMEM;
		goto out_buffers;
	}

	stats_reset(inf);

	return 0;

out_buffers:
	ubx_data_free(inf->sample);
	free(inf->chan);
	free(inf->out);
out_port:
	ubx_port_rm(b, PIN);
out_free:
	free(b->private_data);
	b->private_data = NULL;
	return ret;
}

static int stats_start(ubx_block_t *b)
{
	struct stats_info *inf = (struct stats_info *)b->private_data;

	/* cache ports (the 'stats' port and the runtime 'in' port) */
	inf->p_in = ubx_port_get(b, PIN);
	inf->p_stats = ubx_port_get(b, PSTATS);
	assert(inf->p_in != NULL && inf->p_stats != NULL);

	/* Note: statistics are cumulative and deliberately not reset
	 * here - they persist across stop/start and are only cleared
	 * on (re-)init. */
	return 0;
}

/* fill the scratch output buffer with the current per-channel stats */
static void stats_fill(struct stats_info *inf)
{
	for (long i = 0; i < inf->data_len; i++) {
		struct chan *c = &inf->chan[i];
		struct ubx_stat *s = &inf->out[i];

		s->cnt = inf->cnt;
		s->min = c->min;
		s->max = c->max;
		s->mean = c->mean;
		/* population standard deviation (divide by N) */
		s->std = (inf->cnt > 0) ? sqrt(c->m2 / (double)inf->cnt) : 0;
	}
}

static void stats_log(ubx_block_t *b)
{
	struct stats_info *inf = (struct stats_info *)b->private_data;

	if (inf->cnt == 0) {
		ubx_info(b, "stats: no samples");
		return;
	}

	stats_fill(inf);

	for (long i = 0; i < inf->data_len; i++) {
		struct ubx_stat *s = &inf->out[i];

		if (inf->data_len > 1)
			ubx_info(b, "stats[%ld]: cnt=%lu min=%g max=%g mean=%g std=%g",
				 i, s->cnt, s->min, s->max, s->mean, s->std);
		else
			ubx_info(b, "stats: cnt=%lu min=%g max=%g mean=%g std=%g",
				 s->cnt, s->min, s->max, s->mean, s->std);
	}
}

void stats_step(ubx_block_t *b)
{
	struct stats_info *inf = (struct stats_info *)b->private_data;

	if (__port_read(inf->p_in, inf->sample) <= 0)
		return;		/* NODATA */

	inf->cnt++;

	for (long i = 0; i < inf->data_len; i++) {
		struct chan *c = &inf->chan[i];
		const void *elem = (const char *)inf->sample->data + i * inf->elem_size;
		double x = inf->to_double(elem);
		double delta, delta2;

		/* Welford online update of mean/m2 */
		delta = x - c->mean;
		c->mean += delta / (double)inf->cnt;
		delta2 = x - c->mean;
		c->m2 += delta * delta2;

		if (x < c->min)
			c->min = x;
		if (x > c->max)
			c->max = x;
	}

	/* emit stats, throttled by stats_output_rate if configured */
	if (inf->output_rate_ns != 0) {
		struct ubx_timespec now;
		uint64_t now_ns;

		ubx_gettime(&now);
		now_ns = ubx_ts_to_ns(&now);

		/* output_last_ns == 0 means "never emitted": always emit,
		 * since shortly after boot now_ns itself can be smaller
		 * than output_rate_ns */
		if (inf->output_last_ns != 0 &&
		    now_ns <= inf->output_last_ns + inf->output_rate_ns)
			return;

		inf->output_last_ns = now_ns;
	}

	stats_fill(inf);
	write_ubx_stat_array(inf->p_stats, inf->out, inf->data_len);
}

/* log the accumulated statistics on shutdown */
void stats_stop(ubx_block_t *b)
{
	stats_log(b);
}

void stats_cleanup(ubx_block_t *b)
{
	struct stats_info *inf = (struct stats_info *)b->private_data;

	ubx_data_free(inf->sample);
	free(inf->chan);
	free(inf->out);
	ubx_port_rm(b, PIN);
	free(b->private_data);
	b->private_data = NULL;
}

ubx_proto_block_t stats_comp = {
	.name = "ubx/stats",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = stats_meta,
	.configs = stats_config,
	.ports = stats_ports,

	.init = stats_init,
	.start = stats_start,
	.step = stats_step,
	.stop = stats_stop,
	.cleanup = stats_cleanup,
};

static int stats_mod_init(ubx_node_t *nd)
{
	if (ubx_type_register(nd, &ubx_stat_type) != 0)
		return -1;

	if (ubx_block_register(nd, &stats_comp) != 0) {
		ubx_type_unregister(nd, ubx_stat_type.name);
		return -1;
	}

	return 0;
}

static void stats_mod_cleanup(ubx_node_t *nd)
{
	ubx_block_unregister(nd, "ubx/stats");
	ubx_type_unregister(nd, ubx_stat_type.name);
}

UBX_MODULE_INIT(stats_mod_init)
UBX_MODULE_CLEANUP(stats_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
