/*
 * Generic mux/demux blocks.
 *
 * ubx/mux concatenates 'nin' input ports (in0, in1, ...) into one
 * vector output port 'out'. ubx/demux partitions a vector input port
 * 'in' into 'nout' output ports (out0, out1, ...). By default all
 * numbered ports are scalar; the optional 'in_len'/'out_len' configs
 * assign each a sub-vector length instead, so demux doubles as a
 * slice/splice block: partition the input and connect only the
 * sub-vectors of interest. Both blocks work with any registered type
 * (data is copied bytewise), configured at runtime via the 'type'
 * config.
 *
 * Typical use is composing a vector signal for array-valued blocks
 * (e.g. the measurement input of ubx/kalman) from independent
 * sources, and extracting sub-vectors (e.g. the position part of a
 * state estimate) the other way.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ubx.h"

#define TYPE		"type"
#define NIN		"nin"
#define NOUT		"nout"
#define IN_LEN		"in_len"
#define OUT_LEN		"out_len"
#define PIN		"in"
#define POUT		"out"

/* common instance state for mux and demux */
struct mux_info {
	const ubx_type_t *type;	/* element type */
	long n;			/* number of numbered ports */
	long *lens;		/* per-port sub-vector lengths [n] */
	long *offs;		/* per-port element offsets into the vector [n] */
	long total;		/* vector length (sum of lens) */

	ubx_data_t *sample;	/* vector buffer [total] */
	ubx_port_t **sub_ports; /* the in0../out0.. ports [n] */
	ubx_port_t *vec_port;	/* the vector 'out' resp. 'in' port */
};

/* format the i-th scalar port name into buf */
static const char *scalar_pname(char *buf, size_t size, const char *prefix, long i)
{
	snprintf(buf, size, "%s%ld", prefix, i);
	return buf;
}

/*
 * common init: parse 'type', the port-count config 'cfg_n' and the
 * per-port sub-vector length config 'cfg_len' (unset: all 1; scalar:
 * broadcast; array [n]: individual), compute the offsets and allocate
 * state and the vector buffer. Returns 0 on success.
 */
static int mux_common_init(ubx_block_t *b, const char *cfg_n, const char *cfg_len)
{
	int ret = EINVALID_CONFIG;
	long len;
	const char *type_name;
	const long *n;
	const long *lens;
	struct mux_info *inf;

	b->private_data = calloc(1, sizeof(struct mux_info));

	if (b->private_data == NULL) {
		ubx_err(b, "EOUTOFMEM: failed to alloc mux_info");
		return EOUTOFMEM;
	}

	inf = (struct mux_info *)b->private_data;

	/* type */
	len = cfg_getptr_char(b, TYPE, &type_name);
	assert(len >= 0);

	if (len == 0) {
		ubx_err(b, "EINVALID_CONFIG: mandatory config %s unset", TYPE);
		goto out_free;
	}

	inf->type = ubx_type_get(b->nd, type_name);

	if (inf->type == NULL) {
		ubx_err(b, "unknown type %s", type_name);
		goto out_free;
	}

	/* number of numbered ports */
	len = cfg_getptr_long(b, cfg_n, &n);
	assert(len >= 0);

	if (len == 0) {
		ubx_err(b, "EINVALID_CONFIG: mandatory config %s unset", cfg_n);
		goto out_free;
	}

	if (*n < 1) {
		ubx_err(b, "EINVALID_CONFIG: %s must be >= 1", cfg_n);
		goto out_free;
	}

	inf->n = *n;

	/* per-port sub-vector lengths and offsets */
	len = cfg_getptr_long(b, cfg_len, &lens);
	assert(len >= 0);

	if (len != 0 && len != 1 && len != inf->n) {
		ubx_err(b, "EINVALID_CONFIG_LEN: %s must have 1 or %ld elements (got %ld)",
			cfg_len, inf->n, len);
		goto out_free;
	}

	inf->lens = calloc(inf->n, sizeof(long));
	inf->offs = calloc(inf->n, sizeof(long));

	if (inf->lens == NULL || inf->offs == NULL) {
		ubx_err(b, "EOUTOFMEM: failed to alloc len/off arrays");
		ret = EOUTOFMEM;
		goto out_lens;
	}

	inf->total = 0;

	for (long i = 0; i < inf->n; i++) {
		long l = (len == 0) ? 1 : (len == 1) ? lens[0] : lens[i];

		if (l < 1) {
			ubx_err(b, "EINVALID_CONFIG: %s[%ld] must be >= 1 (got %ld)",
				cfg_len, i, l);
			goto out_lens;
		}

		inf->lens[i] = l;
		inf->offs[i] = inf->total;
		inf->total += l;
	}

	/* allocate the vector buffer and the port ptr array */
	inf->sample = ubx_data_alloc(b->nd, type_name, inf->total);
	inf->sub_ports = calloc(inf->n, sizeof(ubx_port_t *));

	if (inf->sample == NULL || inf->sub_ports == NULL) {
		ubx_err(b, "EOUTOFMEM: failed to alloc buffers");
		ret = EOUTOFMEM;
		goto out_buffers;
	}

	return 0;

out_buffers:
	ubx_data_free(inf->sample);
	free(inf->sub_ports);
out_lens:
	free(inf->lens);
	free(inf->offs);
out_free:
	free(b->private_data);
	b->private_data = NULL;
	return ret;
}

static void mux_common_cleanup(ubx_block_t *b, const char *scalar_prefix,
			       const char *vec_name)
{
	char pname[UBX_PORT_NAME_MAXLEN + 1];
	struct mux_info *inf = (struct mux_info *)b->private_data;

	for (long i = 0; i < inf->n; i++)
		ubx_port_rm(b, scalar_pname(pname, sizeof(pname), scalar_prefix, i));

	ubx_port_rm(b, vec_name);
	ubx_data_free(inf->sample);
	free(inf->sub_ports);
	free(inf->lens);
	free(inf->offs);
	free(b->private_data);
	b->private_data = NULL;
}

/* cache the port pointers (common start) */
static int mux_common_start(ubx_block_t *b, const char *scalar_prefix,
			    const char *vec_name)
{
	char pname[UBX_PORT_NAME_MAXLEN + 1];
	struct mux_info *inf = (struct mux_info *)b->private_data;

	for (long i = 0; i < inf->n; i++) {
		inf->sub_ports[i] =
			ubx_port_get(b, scalar_pname(pname, sizeof(pname),
						     scalar_prefix, i));
		assert(inf->sub_ports[i] != NULL);
	}

	inf->vec_port = ubx_port_get(b, vec_name);
	assert(inf->vec_port != NULL);

	return 0;
}

/*
 * mux
 */

char mux_meta[] =
	"{ doc='concatenate N input ports into one vector output', realtime=true }";

ubx_proto_config_t mux_config[] = {
	{ .name = TYPE, .type_name = "char", .min = 1, .doc = "ubx type name of the signal (any registered type)" },
	{ .name = NIN, .type_name = "long", .min = 1, .max = 1, .doc = "number of input ports in0..in<nin-1>" },
	{ .name = IN_LEN, .type_name = "long", .doc = "sub-vector length per input: scalar or [nin] (default 1)" },
	{ .name = "loglevel", .type_name = "int" },
	{ 0 },
};

/* all ports are added at runtime in init */
ubx_proto_port_t mux_ports[] = {
	{ 0 },
};

static int mux_init(ubx_block_t *b)
{
	int ret;
	long i;
	char pname[UBX_PORT_NAME_MAXLEN + 1];
	struct mux_info *inf;

	ret = mux_common_init(b, NIN, IN_LEN);

	if (ret != 0)
		return ret;

	inf = (struct mux_info *)b->private_data;

	for (i = 0; i < inf->n; i++) {
		ret = ubx_inport_add(b, scalar_pname(pname, sizeof(pname), PIN, i),
				     "input", 0, inf->type->name, inf->lens[i]);
		if (ret != 0)
			goto out_ports;
	}

	ret = ubx_outport_add(b, POUT, "muxed vector output", 0,
			      inf->type->name, inf->total);

	if (ret != 0)
		goto out_ports;

	return 0;

out_ports:
	for (i--; i >= 0; i--)
		ubx_port_rm(b, scalar_pname(pname, sizeof(pname), PIN, i));
	ubx_data_free(inf->sample);
	free(inf->sub_ports);
	free(inf->lens);
	free(inf->offs);
	free(b->private_data);
	b->private_data = NULL;
	return ret;
}

static int mux_start(ubx_block_t *b)
{
	return mux_common_start(b, PIN, POUT);
}

void mux_step(ubx_block_t *b)
{
	int updated = 0;
	struct mux_info *inf = (struct mux_info *)b->private_data;

	/* read each input into its slot of the vector buffer; slots
	 * without new data keep their last value (zeros initially) */
	for (long i = 0; i < inf->n; i++) {
		ubx_data_t elem = {
			.type = inf->type,
			.len = inf->lens[i],
			.data = (char *)inf->sample->data + inf->offs[i] * inf->type->size,
		};

		if (__port_read(inf->sub_ports[i], &elem) > 0)
			updated = 1;
	}

	/* only emit a vector if at least one input had new data */
	if (updated)
		__port_write(inf->vec_port, inf->sample);
}

void mux_cleanup(ubx_block_t *b)
{
	mux_common_cleanup(b, PIN, POUT);
}

ubx_proto_block_t mux_comp = {
	.name = "ubx/mux",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = mux_meta,
	.configs = mux_config,
	.ports = mux_ports,

	.init = mux_init,
	.start = mux_start,
	.step = mux_step,
	.cleanup = mux_cleanup,
};

/*
 * demux
 */

char demux_meta[] =
	"{ doc='partition a vector input port into N outputs', realtime=true }";

ubx_proto_config_t demux_config[] = {
	{ .name = TYPE, .type_name = "char", .min = 1, .doc = "ubx type name of the signal (any registered type)" },
	{ .name = NOUT, .type_name = "long", .min = 1, .max = 1, .doc = "number of output ports out0..out<nout-1>" },
	{ .name = OUT_LEN, .type_name = "long", .doc = "sub-vector length per output: scalar or [nout] (default 1)" },
	{ .name = "loglevel", .type_name = "int" },
	{ 0 },
};

/* all ports are added at runtime in init */
ubx_proto_port_t demux_ports[] = {
	{ 0 },
};

static int demux_init(ubx_block_t *b)
{
	int ret;
	long i;
	char pname[UBX_PORT_NAME_MAXLEN + 1];
	struct mux_info *inf;

	ret = mux_common_init(b, NOUT, OUT_LEN);

	if (ret != 0)
		return ret;

	inf = (struct mux_info *)b->private_data;

	for (i = 0; i < inf->n; i++) {
		ret = ubx_outport_add(b, scalar_pname(pname, sizeof(pname), POUT, i),
				      "output", 0, inf->type->name, inf->lens[i]);
		if (ret != 0)
			goto out_ports;
	}

	ret = ubx_inport_add(b, PIN, "vector input to demux", 0,
			     inf->type->name, inf->total);

	if (ret != 0)
		goto out_ports;

	return 0;

out_ports:
	for (i--; i >= 0; i--)
		ubx_port_rm(b, scalar_pname(pname, sizeof(pname), POUT, i));
	ubx_data_free(inf->sample);
	free(inf->sub_ports);
	free(inf->lens);
	free(inf->offs);
	free(b->private_data);
	b->private_data = NULL;
	return ret;
}

static int demux_start(ubx_block_t *b)
{
	return mux_common_start(b, POUT, PIN);
}

void demux_step(ubx_block_t *b)
{
	long len;
	struct mux_info *inf = (struct mux_info *)b->private_data;

	len = __port_read(inf->vec_port, inf->sample);

	if (len <= 0)
		return;		/* NODATA */

	if (len != inf->total) {
		ubx_err(b, "in value has wrong length (got %ld, expected %ld)",
			len, inf->total);
		return;
	}

	for (long i = 0; i < inf->n; i++) {
		ubx_data_t elem = {
			.type = inf->type,
			.len = inf->lens[i],
			.data = (char *)inf->sample->data + inf->offs[i] * inf->type->size,
		};

		__port_write(inf->sub_ports[i], &elem);
	}
}

void demux_cleanup(ubx_block_t *b)
{
	mux_common_cleanup(b, POUT, PIN);
}

ubx_proto_block_t demux_comp = {
	.name = "ubx/demux",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = demux_meta,
	.configs = demux_config,
	.ports = demux_ports,

	.init = demux_init,
	.start = demux_start,
	.step = demux_step,
	.cleanup = demux_cleanup,
};

/*
 * module
 */

static int mux_mod_init(ubx_node_t *nd)
{
	if (ubx_block_register(nd, &mux_comp) != 0)
		return -1;

	if (ubx_block_register(nd, &demux_comp) != 0) {
		ubx_block_unregister(nd, "ubx/mux");
		return -1;
	}

	return 0;
}

static void mux_mod_cleanup(ubx_node_t *nd)
{
	ubx_block_unregister(nd, "ubx/demux");
	ubx_block_unregister(nd, "ubx/mux");
}

UBX_MODULE_INIT(mux_mod_init)
UBX_MODULE_CLEANUP(mux_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
