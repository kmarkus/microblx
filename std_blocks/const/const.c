/*
 * A generic constant value c- and i-block
 */

#define DEBUG

#include <stdio.h>
#include <stdlib.h>

#include "ubx.h"

#define TYPE_NAME	"type_name"
#define DATA_LEN	"data_len"
#define VALUE		"value"
#define OUT		"out"

#ifdef BUILD_IBLOCK
const char iconst_meta[] = "{ doc='const value i-block', realtime=true }";
#else
const char cconst_meta[] = "{ doc='const value c-block', realtime=true }";
#endif

ubx_proto_config_t const_config[] = {
	{ .name=TYPE_NAME, .type_name="char", .doc="ubx type name of the value to output" },
	{ .name=DATA_LEN, .type_name="long", .doc="data length of the value to output", .min=0, .max=1 },
	{ 0 },
};

struct const_info {
	ubx_type_t *type;
	const void *value;
	long data_len;

	const ubx_data_t *data;
#ifndef BUILD_IBLOCK
	ubx_port_t *p_out;
#endif
};


static int const_init(ubx_block_t *b)
{
	int ret = EINVALID_CONFIG;
	long len;
	const long *data_len;
	const char *type_name;
	struct const_info *inf;

	b->private_data = calloc(1, sizeof(struct const_info));

	if (b->private_data == NULL) {
		ubx_err(b, "EOUTOFMEM: failed to alloc const_infoo");
		ret = EOUTOFMEM;
		goto out;
	}

	inf = (struct const_info *)b->private_data;

	/* type_name */
	len = cfg_getptr_char(b, TYPE_NAME, &type_name);
	assert(len >= 0);

	if (len == 0) {
		ubx_err(b, "EINVALID_CONFIG: mandatory config %s unset", TYPE_NAME);
		goto out_free;
	}

	inf->type = ubx_type_get(b->nd, type_name);

	if (inf->type == NULL) {
		ubx_err(b, "unknown type %s", type_name);
		goto out_free;
	}

	/* data_len */
	len = cfg_getptr_long(b, DATA_LEN, &data_len);
	assert(len >= 0);
	inf->data_len = (len>0) ? *data_len : 1;

	/* add config 'value' */
	ret = ubx_config_add(b, VALUE, NULL, type_name);

	if (ret != 0)
		goto out_free;

#ifndef BUILD_IBLOCK
	/* add port 'out' */
	ret = ubx_outport_add(b, OUT, "const value output port", 0,
			      type_name, inf->data_len);

	if (ret != 0)
		goto out_free;
#endif

	/* all good */
	ret = 0;
	goto out;

out_free:
	free(b->private_data);
out:
	return ret;
}

static int const_start(ubx_block_t *b)
{
	struct const_info *inf = (struct const_info *)b->private_data;

	inf->data = ubx_config_get_data(b, VALUE);
	assert(inf->data != NULL);

	if (inf->data->len != inf->data_len) {
		ubx_err(b, "EINVALID_CONFIG_LEN for 'value': expected %lu, got %lu",
			inf->data_len, inf->data->len);
		return EINVALID_CONFIG_LEN;
	}

#ifndef BUILD_IBLOCK
	/* cache out port */
	inf->p_out = ubx_port_get(b, OUT);
	assert(inf->p_out != NULL);
#endif
	return 0;
}

#ifdef BUILD_IBLOCK
static long const_read(ubx_block_t *i, ubx_data_t *data)
{
	struct const_info *inf = (struct const_info *)i->private_data;

	if (inf->type != data->type) {
		ubx_err(i, "%s: invalid read dest type %s (should be %s)",
			__func__, data->type->name, inf->data->type->name);
		return EINVALID_TYPE;
	}

	if (inf->data_len != data->len) {
		ubx_err(i, "%s:	invalid read dest len %lu (expected %lu)",
			__func__, data->len, inf->data_len);
	}

	memcpy(data->data, inf->data->data, data_size(data));
	return inf->data_len;
}
#else /* cblock */
void const_step(ubx_block_t *b)
{
	struct const_info *inf = (struct const_info *)b->private_data;
	__port_write(inf->p_out, inf->data);
}
#endif /* BUILD_IBLOCK */

void const_cleanup(ubx_block_t *b)
{
	ubx_port_rm(b, OUT);
	ubx_config_rm(b, VALUE);
	free(b->private_data);
}

/* put everything together */
ubx_proto_block_t const_comp = {
#ifdef BUILD_IBLOCK
	.name = "ubx/iconst",
	.type = BLOCK_TYPE_INTERACTION,
	.read = const_read,
	.meta_data = iconst_meta,
#else
	.name = "ubx/cconst",
	.type = BLOCK_TYPE_COMPUTATION,
	.step = const_step,
	.meta_data = cconst_meta,
#endif
	.configs = const_config,

	/* ops */
	.init = const_init,
	.start = const_start,
	.cleanup = const_cleanup,
};

static int const_mod_init(ubx_node_t *nd)
{
	return ubx_block_register(nd, &const_comp);
}

static void const_mod_cleanup(ubx_node_t *nd)
{
#ifdef BUILD_IBLOCK
	ubx_block_unregister(nd, "ubx/iconst");
#else
	ubx_block_unregister(nd, "ubx/cconst");
#endif
}

UBX_MODULE_INIT(const_mod_init)
UBX_MODULE_CLEANUP(const_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
