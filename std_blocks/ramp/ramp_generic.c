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
 * unsigned ramps wrap as expected. The 'start'/'slope' configs have
 * the native type too: preinit adds them, after 'type' is known.
 */

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
	{ .name = TYPE, .type_name = "char", .min = 1, .attrs = CONFIG_ATTR_PREINIT,
	  .doc = "ubx numeric type name of the output and of start/slope" },
	{ .name = DATA_LEN, .type_name = "long", .min = 0, .max = 1, .attrs = CONFIG_ATTR_PREINIT,
	  .doc = "vector length (default 1)" },
	/* start and slope are added by preinit with the configured type */
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

/* per-type increment kernels, selected once at init */
typedef void (*add_fn)(void *cur, const void *slope, long n);

#define DEF_RAMP_ADD(SFX, T)						\
	static void add_##SFX(void *cur, const void *slope, long n)	\
	{								\
		T *c = (T *)cur;					\
		const T *s = (const T *)slope;				\
									\
		for (long i = 0; i < n; i++)				\
			c[i] += s[i];					\
	}

DEF_RAMP_ADD(i8,  int8_t)
DEF_RAMP_ADD(i16, int16_t)
DEF_RAMP_ADD(i32, int32_t)
DEF_RAMP_ADD(i64, int64_t)
DEF_RAMP_ADD(u8,  uint8_t)
DEF_RAMP_ADD(u16, uint16_t)
DEF_RAMP_ADD(u32, uint32_t)
DEF_RAMP_ADD(u64, uint64_t)
DEF_RAMP_ADD(flt, float)
DEF_RAMP_ADD(dbl, double)

struct numtype {
	const char *name;
	long size;
	add_fn add;
};

static const struct numtype numtypes[] = {
	{ "int8_t",	sizeof(int8_t),		add_i8 },
	{ "int16_t",	sizeof(int16_t),	add_i16 },
	{ "int32_t",	sizeof(int32_t),	add_i32 },
	{ "int64_t",	sizeof(int64_t),	add_i64 },
	{ "uint8_t",	sizeof(uint8_t),	add_u8 },
	{ "uint16_t",	sizeof(uint16_t),	add_u16 },
	{ "uint32_t",	sizeof(uint32_t),	add_u32 },
	{ "uint64_t",	sizeof(uint64_t),	add_u64 },
	{ "float",	sizeof(float),		add_flt },
	{ "double",	sizeof(double),		add_dbl },
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

/* look up and check the type and data_len configs */
static int get_type(ubx_block_t *b, const struct numtype **nt, const char **type_name,
		    long *data_len)
{
	long len;
	const long *dl;

	len = cfg_getptr_char(b, TYPE, type_name);
	assert(len >= 0);

	if (len == 0) {
		ubx_err(b, "EINVALID_CONFIG: mandatory config %s unset", TYPE);
		return -1;
	}

	if (ubx_type_get(b->nd, *type_name) == NULL) {
		ubx_err(b, "unknown type %s", *type_name);
		return -1;
	}

	*nt = lookup_conv(*type_name);

	if (*nt == NULL) {
		ubx_err(b, "EINVALID_CONFIG: %s '%s' is not a supported numeric type",
			TYPE, *type_name);
		return -1;
	}

	len = cfg_getptr_long(b, DATA_LEN, &dl);
	assert(len >= 0);
	*data_len = (len > 0) ? *dl : 1;

	if (*data_len < 1) {
		ubx_err(b, "EINVALID_CONFIG: %s must be >= 1", DATA_LEN);
		return -1;
	}

	return 0;
}

/* add start and slope with the configured type */
static int rampg_preinit(ubx_block_t *b)
{
	int ret;
	long data_len;
	const char *type_name;
	const struct numtype *nt;

	if (get_type(b, &nt, &type_name, &data_len) != 0)
		return EINVALID_CONFIG;

	ret = ubx_config_add(b, START, "start value: scalar or per-element [data_len] (default 0)",
			     type_name);
	if (ret != 0)
		return ret;

	ret = ubx_config_add(b, SLOPE, "increment per step: scalar or per-element [data_len]",
			     type_name);
	if (ret != 0)
		ubx_config_rm(b, START);

	return ret;
}

static void rampg_preexit(ubx_block_t *b)
{
	ubx_config_rm(b, START);
	ubx_config_rm(b, SLOPE);
}

/*
 * copy a native config into dst[data_len], broadcasting a scalar.
 * Unset is OK (dst stays zeroed) unless mandatory.
 */
static int load_cfg(ubx_block_t *b, const char *name, const struct numtype *nt,
		    void *dst, long data_len, int mandatory)
{
	void *v;
	long len = ubx_config_get_data_ptr(b, name, &v);

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

	if (len == data_len) {
		memcpy(dst, v, data_len * nt->size);
		return 0;
	}

	for (long i = 0; i < data_len; i++)
		memcpy((char *)dst + i * nt->size, v, nt->size);

	return 0;
}

static int rampg_init(ubx_block_t *b)
{
	int ret = EINVALID_CONFIG;
	const char *type_name;
	struct rampg_info *inf;

	b->private_data = calloc(1, sizeof(struct rampg_info));

	if (b->private_data == NULL) {
		ubx_err(b, "EOUTOFMEM: failed to alloc rampg_info");
		return EOUTOFMEM;
	}

	inf = (struct rampg_info *)b->private_data;

	if (get_type(b, &inf->nt, &type_name, &inf->data_len) != 0)
		goto out_free;

	/* allocate the native start/slope arrays and the write buffer */
	inf->start = calloc(inf->data_len, inf->nt->size);
	inf->slope = calloc(inf->data_len, inf->nt->size);
	inf->cur = ubx_data_alloc(b->nd, type_name, inf->data_len);

	if (inf->start == NULL || inf->slope == NULL || inf->cur == NULL) {
		ubx_err(b, "EOUTOFMEM: failed to alloc buffers");
		ret = EOUTOFMEM;
		goto out_buffers;
	}

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

	.preinit = rampg_preinit,
	.preexit = rampg_preexit,
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
