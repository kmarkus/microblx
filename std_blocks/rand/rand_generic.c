/*
 * A generic, runtime-typed random number generator block.
 *
 * Each step a fresh random sample (a vector for 'data_len' > 1) is
 * written to 'out'. The numeric type is configurable at runtime via
 * the 'type' config: floating-point types are uniform in [0, 1),
 * integer types are uniform over the full type range.
 *
 * Unlike the legacy compile-time typed ubx/rand_<type> variants (kept
 * for backwards compatibility), the PRNG state is *per instance*
 * (erand48/jrand48 family): instances do not race on the global
 * drand48 state across trigger threads and seeding one block does not
 * re-seed the others, so sequences are reproducible per instance.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ubx.h"

#define TYPE		"type"
#define DATA_LEN	"data_len"
#define SEED		"seed"
#define POUT		"out"

char randg_meta[] =
	"{ doc='runtime-typed random number generator (per-instance PRNG state)', realtime=true }";

ubx_proto_config_t randg_config[] = {
	{ .name = TYPE, .type_name = "char", .min = 1, .doc = "ubx numeric type name of the output" },
	{ .name = DATA_LEN, .type_name = "long", .min = 0, .max = 1, .doc = "vector length (default 1)" },
	{ .name = SEED, .type_name = "long", .min = 0, .max = 1, .doc = "seed of this instance's PRNG (default 0)" },
	/* if a 'loglevel' config is defined, it will automatically
	 * affect the block loglevel. If unset the global loglevel is used */
	{ .name = "loglevel", .type_name = "int" },
	{ 0 },
};

/* Note: the 'out' port is added at runtime in init with the configured
 * type. */
ubx_proto_port_t randg_ports[] = {
	{ 0 },
};

/* 32 and 64 random bits from the per-instance state */
static uint32_t rnd_u32(unsigned short *x)
{
	return (uint32_t)jrand48(x);
}

static uint64_t rnd_u64(unsigned short *x)
{
	return ((uint64_t)rnd_u32(x) << 32) | rnd_u32(x);
}

/* per-type fill kernels, selected once at init */
typedef void (*fill_fn)(void *dst, long n, unsigned short *x);

#define DEF_RAND_FILL(SFX, T, EXPR)					\
	static void fill_##SFX(void *dst, long n, unsigned short *x)	\
	{								\
		T *d = (T *)dst;					\
									\
		for (long i = 0; i < n; i++)				\
			d[i] = (T)(EXPR);				\
	}

DEF_RAND_FILL(i8,  int8_t,   rnd_u32(x))
DEF_RAND_FILL(i16, int16_t,  rnd_u32(x))
DEF_RAND_FILL(i32, int32_t,  rnd_u32(x))
DEF_RAND_FILL(i64, int64_t,  rnd_u64(x))
DEF_RAND_FILL(u8,  uint8_t,  rnd_u32(x))
DEF_RAND_FILL(u16, uint16_t, rnd_u32(x))
DEF_RAND_FILL(u32, uint32_t, rnd_u32(x))
DEF_RAND_FILL(u64, uint64_t, rnd_u64(x))
DEF_RAND_FILL(flt, float,    erand48(x))
DEF_RAND_FILL(dbl, double,   erand48(x))

struct numtype {
	const char *name;
	fill_fn fill;
};

static const struct numtype numtypes[] = {
	{ "int8_t",	fill_i8 },
	{ "int16_t",	fill_i16 },
	{ "int32_t",	fill_i32 },
	{ "int64_t",	fill_i64 },
	{ "uint8_t",	fill_u8 },
	{ "uint16_t",	fill_u16 },
	{ "uint32_t",	fill_u32 },
	{ "uint64_t",	fill_u64 },
	{ "float",	fill_flt },
	{ "double",	fill_dbl },
};

static const struct numtype *lookup_conv(const char *name)
{
	for (unsigned int i = 0; i < ARRAY_SIZE(numtypes); i++)
		if (strcmp(numtypes[i].name, name) == 0)
			return &numtypes[i];
	return NULL;
}

struct randg_info {
	const struct numtype *nt;
	long data_len;		/* number of elements (vector length) */

	unsigned short xsubi[3]; /* per-instance PRNG state */

	ubx_data_t *sample;	/* write buffer [data_len] */
	ubx_port_t *p_out;
};

static int randg_init(ubx_block_t *b)
{
	int ret = EINVALID_CONFIG;
	long len;
	const char *type_name;
	const long *data_len;
	const long *seed;
	long seedval;
	struct randg_info *inf;

	b->private_data = calloc(1, sizeof(struct randg_info));

	if (b->private_data == NULL) {
		ubx_err(b, "EOUTOFMEM: failed to alloc randg_info");
		return EOUTOFMEM;
	}

	inf = (struct randg_info *)b->private_data;

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

	/* seed the per-instance state like srand48 would (so a
	 * 'double' instance reproduces the legacy drand48 sequence) */
	len = cfg_getptr_long(b, SEED, &seed);
	assert(len >= 0);
	seedval = (len > 0) ? *seed : 0;

	inf->xsubi[0] = 0x330e;
	inf->xsubi[1] = (unsigned short)(seedval & 0xffff);
	inf->xsubi[2] = (unsigned short)((seedval >> 16) & 0xffff);

	/* allocate the write buffer and add the runtime-typed port */
	inf->sample = ubx_data_alloc(b->nd, type_name, inf->data_len);

	if (inf->sample == NULL) {
		ubx_err(b, "EOUTOFMEM: failed to alloc sample buffer");
		ret = EOUTOFMEM;
		goto out_free;
	}

	ret = ubx_outport_add(b, POUT, "random output", 0, type_name, inf->data_len);

	if (ret != 0)
		goto out_sample;

	return 0;

out_sample:
	ubx_data_free(inf->sample);
out_free:
	free(b->private_data);
	b->private_data = NULL;
	return ret;
}

static int randg_start(ubx_block_t *b)
{
	struct randg_info *inf = (struct randg_info *)b->private_data;

	inf->p_out = ubx_port_get(b, POUT);
	assert(inf->p_out != NULL);

	/* Note: the PRNG state is deliberately not re-seeded here - the
	 * sequence continues across stop/start and only re-init
	 * restarts it from the seed. */
	return 0;
}

void randg_step(ubx_block_t *b)
{
	struct randg_info *inf = (struct randg_info *)b->private_data;

	inf->nt->fill(inf->sample->data, inf->data_len, inf->xsubi);
	__port_write(inf->p_out, inf->sample);
}

void randg_cleanup(ubx_block_t *b)
{
	struct randg_info *inf = (struct randg_info *)b->private_data;

	ubx_data_free(inf->sample);
	ubx_port_rm(b, POUT);
	free(b->private_data);
	b->private_data = NULL;
}

ubx_proto_block_t randg_comp = {
	.name = "ubx/rand",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = randg_meta,
	.configs = randg_config,
	.ports = randg_ports,

	.init = randg_init,
	.start = randg_start,
	.step = randg_step,
	.cleanup = randg_cleanup,
};

static int randg_mod_init(ubx_node_t *nd)
{
	return ubx_block_register(nd, &randg_comp);
}

static void randg_mod_cleanup(ubx_node_t *nd)
{
	ubx_block_unregister(nd, "ubx/rand");
}

UBX_MODULE_INIT(randg_mod_init)
UBX_MODULE_CLEANUP(randg_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
