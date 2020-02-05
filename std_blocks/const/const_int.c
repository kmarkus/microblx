/*
 * An constant value interaction.
 */

/* #define DEBUG 1 */

#include <stdio.h>
#include <stdlib.h>

/* #define DEBUG 1 */
#include "ubx.h"

UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)

char const_int_meta[] =
	"{ doc='const int interaction',"
	"  realtime=true,"
	"}";

ubx_config_t const_int_config[] = {
	{ .name="value", .type_name = "int", .min=1, .max=1 },
	{ NULL },
};

struct blk_inf {
	ubx_type_t *type;
	const int* value;
};


static int const_int_init(ubx_block_t *i)
{
	int ret = -1;
	struct blk_inf *inf;

	if((i->private_data = calloc(1, sizeof(struct blk_inf)))==NULL) {
		ubx_err(i, "failed to alloc blk_info");
		ret = EOUTOFMEM;
		goto out;
	}


	inf = (struct blk_inf*) i->private_data;

	inf->type = ubx_type_get(i->ni, "int");

	if(inf->type == NULL) {
		ubx_err(i, "failed to lookup type int");
		ret = EINVALID_CONFIG;
		goto out;
	}
	ret = 0;
out:
	return ret;
}

static int const_int_start(ubx_block_t *i)
{
	long len;
	struct blk_inf *inf;

	inf = (struct blk_inf*) i->private_data;

	if ((len = cfg_getptr_int(i, "value", &inf->value)) < 0) {
		ubx_err(i, "missing config 'value'");
		return EINVALID_CONFIG;
	}

	return 0;
}

static void const_int_cleanup(ubx_block_t *i)
{
	free(i->private_data);
}

static long const_int_read(ubx_block_t *i, ubx_data_t* data)
{
	int ret = 0;
	struct blk_inf *inf;
	inf = (struct blk_inf*) i->private_data;

	if(inf->type != data->type) {
		ubx_err(i, "invalid read destination type %s (should be int)",
			data->type->name);
		goto out;
	}

	assert(sizeof(int) == data_size(data));
	memcpy(data->data, inf->value, sizeof(int));
	ret = 1;
out:
	return ret;
}


/* put everything together */
ubx_block_t const_int_comp = {
	.name = "consts/const_int",
	.type = BLOCK_TYPE_INTERACTION,
	.meta_data = const_int_meta,
	.configs = const_int_config,

	/* ops */
	.init = const_int_init,
	.start = const_int_start,
	.cleanup = const_int_cleanup,
	.read = const_int_read,
};

static int const_int_mod_init(ubx_node_info_t* ni)
{
	return ubx_block_register(ni, &const_int_comp);
}

static void const_int_mod_cleanup(ubx_node_info_t *ni)
{
	ubx_block_unregister(ni, "const_int/const_int");
}

UBX_MODULE_INIT(const_int_mod_init)
UBX_MODULE_CLEANUP(const_int_mod_cleanup)
