/*
 * rand microblx function block
 */

#if RAND_FLOAT_T == 1
# define RAND_T float
#elif RAND_DOUBLE_T == 1
# define RAND_T double
#elif RAND_UINT32_T == 1
# define RAND_T uint32_t
# define BLOCK_NAME "ubx/rand_uint32"
#elif RAND_INT32_T == 1
# define RAND_T int32_t
# define BLOCK_NAME "ubx/rand_int32"
#else
# error "no type defined"
#endif

#ifndef BLOCK_NAME
# define BLOCK_NAME "ubx/rand_" QUOTE(RAND_T)
#endif

#include <stdlib.h>
#include <string.h>
#include <ubx.h>

/* block meta information */
char rand_meta[] =
	" { doc='" QUOTE(RAND_T) " random number generator block',"
	"   realtime=true,"
	"}";

/* declaration of block configuration */
#define SEED 	"seed"

ubx_proto_config_t rand_config[] = {
	{ .name = SEED, .type_name = "long", .doc = "seed to initialize with" },
	{ 0 },
};

/* declaration port block ports */
#define OUT	"out"
ubx_proto_port_t rand_ports[] = {
	{ .name = OUT, .out_type_name = QUOTE(RAND_T), .out_data_len = 1, .doc = "rand generator output"  },
	{ 0 },
};

/* define a structure for holding the block local state. By assigning an
 * instance of this struct to the block private_data pointer (see init), this
 * information becomes accessible within the hook functions.
 */
struct rand_info {
	ubx_port_t *p_out;
};


/* init */
int rand_init(ubx_block_t *b)
{
	long len;
	const long *seed;
	struct rand_info *inf;

	/* allocate memory for the block local state */
	b->private_data = calloc(1, sizeof(struct rand_info));
	inf = (struct rand_info *)b->private_data;

	if (b->private_data == NULL) {
		ubx_err(b, "rand: failed to alloc memory");
		return EOUTOFMEM;
	}

	/* seed */
	len = cfg_getptr_long(b, SEED, &seed);
	assert(len >= 0);
	srand48((len > 0) ? *seed : 0);

	inf->p_out = ubx_port_get(b, OUT);
	assert(inf->p_out);

	return 0;
}


/* cleanup */
void rand_cleanup(ubx_block_t *b)
{
	free(b->private_data);
}

/* step */
void rand_step(ubx_block_t *b)
{
	struct rand_info *inf = (struct rand_info *)b->private_data;
#if RAND_FLOAT_T == 1
	RAND_T val = drand48();
	write_float(inf->p_out,	&val);
#elif RAND_DOUBLE_T == 1
	double val = drand48();
	write_double(inf->p_out, &val);
#elif RAND_UINT32_T == 1
	uint32_t val = lrand48();
	write_uint32(inf->p_out, &val);
#elif RAND_INT32_T == 1
	int32_t val = mrand48();
	write_int32(inf->p_out, &val);
#else
# error "unsupported type " QUOTE(RAND_T)
#endif


}


ubx_proto_block_t rand_block = {
	.name = BLOCK_NAME,
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = rand_meta,
	.configs = rand_config,
	.ports = rand_ports,

	.init = rand_init,
	.cleanup = rand_cleanup,
	.step = rand_step,
};


int rand_mod_init(ubx_node_t *nd)
{
	return ubx_block_register(nd, &rand_block);
}

void rand_mod_cleanup(ubx_node_t *nd)
{
	ubx_block_unregister(nd, BLOCK_NAME);
}

UBX_MODULE_INIT(rand_mod_init)
UBX_MODULE_CLEANUP(rand_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
