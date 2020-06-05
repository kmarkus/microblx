/*
 * A simple random number generator function block.
 *
 * Note: this is just an didactical/testing block.
 *
 * If you need a real random block, use the rand_* blocks instead.
 */

#undef UBX_DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "ubx.h"

#include "types/random_config.h"
#include "types/random_config.h.hexarr"

ubx_type_t random_config_type = def_struct_type(struct random_config,
						&random_config_h);

def_cfg_getptr_fun(cfg_getptr_random_config, struct random_config)

char rnd_meta[] =
	"{ doc='A random number generator function block',"
	"  realtime=true,"
	"}";

ubx_config_t rnd_config[] = {
	{ .name = "loglevel", .type_name = "int" },
	{ .name = "min_max_config", .type_name = "struct random_config", .min = 1, .max = 1 },
	{ 0 },
};

ubx_port_t rnd_ports[] = {
	{ .name = "seed", .in_type_name = "unsigned int" },
	{ .name = "rnd", .out_type_name = "unsigned int" },
	{ 0 },
};

struct random_info {
	unsigned int min;
	unsigned int max;
};


int rnd_init(ubx_block_t *b)
{
	b->private_data = calloc(1, sizeof(struct random_info));

	if (b->private_data == NULL) {
		ubx_crit(b, "__func__: ENOMEM");
		return EOUTOFMEM;
	}

	return 0;
}

void rnd_cleanup(ubx_block_t *b)
{
	free(b->private_data);
}

int rnd_start(ubx_block_t *b)
{
	uint32_t seed, ret;
	long len;
	const struct random_config *rndconf;
	struct random_info *inf;

	inf = (struct random_info *)b->private_data;

	/* get and store min_max_config */
	len = cfg_getptr_random_config(b, "min_max_config", &rndconf);

	if (len < 0) {
		ubx_err(b, "failed to retrieve min_max_config");
		return -1;
	} else if (len == 0) {
		inf->min = 0;
		inf->max = INT_MAX;
	} else {
		inf->min = rndconf->min;
		inf->max = rndconf->max;
	}

	/* seed is allowed to change at runtime, check if one is already available */
	ubx_port_t *seed_port = ubx_port_get(b, "seed");

	ret = read_uint(seed_port, &seed);

	if (ret > 0) {
		ubx_info(b, "%s: seed: %d, min: %d, max: %d",
			 __func__, seed, inf->min, inf->max);
		srandom(seed);
	} else {
		ubx_info(b, "%s: min: %d, max: %d",
			 __func__, inf->min, inf->max);
	}

	return 0;
}

void rnd_step(ubx_block_t *b)
{
	unsigned int rand_val;
	struct random_info *inf = (struct random_info *)b->private_data;
	ubx_port_t *rand_port = ubx_port_get(b, "rnd");

	rand_val = (unsigned int)random();
	rand_val = (rand_val > inf->max) ? (rand_val % inf->max) : rand_val;
	rand_val = (rand_val < inf->min) ? ((inf->min + rand_val) % inf->max) : rand_val;

	write_uint(rand_port, &rand_val);
}


ubx_block_t random_comp = {
	.name = "random/random",
	.meta_data = rnd_meta,
	.type = BLOCK_TYPE_COMPUTATION,

	.ports = rnd_ports,
	.configs = rnd_config,

	.init = rnd_init,
	.start = rnd_start,
	.cleanup = rnd_cleanup,
	.step = rnd_step,
};

int rnd_module_init(ubx_node_info_t *ni)
{
	if (ubx_type_register(ni, &random_config_type))
		return -1;
	return ubx_block_register(ni, &random_comp);
}

void rnd_module_cleanup(ubx_node_info_t *ni)
{
	ubx_type_unregister(ni, "struct random_config");
	ubx_block_unregister(ni, "random/random");
}

UBX_MODULE_INIT(rnd_module_init)
UBX_MODULE_CLEANUP(rnd_module_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
