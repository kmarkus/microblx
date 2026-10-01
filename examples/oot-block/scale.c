/*
 * scale: out = gain * in + offset
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <ubx.h>

#include "types/scale_config.h"
#include "types/scale_config.h.hexarr"

ubx_type_t scale_config_type = def_struct_type(struct scale_config, &scale_config_h);

def_cfg_getptr_fun(cfg_getptr_scale_config, struct scale_config)

char scale_meta[] = "{ doc='scale a signal: out = gain * in + offset', realtime=true }";

ubx_proto_config_t scale_config[] = {
	{ .name = "scale", .type_name = "struct scale_config", .min = 1, .max = 1 },
	{ 0 },
};

ubx_proto_port_t scale_ports[] = {
	{ .name = "in", .in_type_name = "double" },
	{ .name = "out", .out_type_name = "double" },
	{ 0 },
};

struct scale_info {
	const struct scale_config *cfg;
	ubx_port_t *p_in;
	ubx_port_t *p_out;
};

int scale_init(ubx_block_t *b)
{
	b->private_data = calloc(1, sizeof(struct scale_info));

	if (b->private_data == NULL) {
		ubx_err(b, "EOUTOFMEM");
		return EOUTOFMEM;
	}
	return 0;
}

int scale_start(ubx_block_t *b)
{
	struct scale_info *inf = b->private_data;

	if (cfg_getptr_scale_config(b, "scale", &inf->cfg) != 1)
		return -1;

	inf->p_in = ubx_port_get(b, "in");
	inf->p_out = ubx_port_get(b, "out");
	return 0;
}

void scale_step(ubx_block_t *b)
{
	struct scale_info *inf = b->private_data;
	double val;

	if (read_double(inf->p_in, &val) <= 0)
		return;

	val = inf->cfg->gain * val + inf->cfg->offset;
	write_double(inf->p_out, &val);
}

void scale_cleanup(ubx_block_t *b)
{
	free(b->private_data);
}

ubx_proto_block_t scale_block = {
	.name = "oot/scale",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = scale_meta,
	.configs = scale_config,
	.ports = scale_ports,

	.init = scale_init,
	.start = scale_start,
	.step = scale_step,
	.cleanup = scale_cleanup,
};

int scale_mod_init(ubx_node_t *nd)
{
	if (ubx_type_register(nd, &scale_config_type))
		return -1;
	return ubx_block_register(nd, &scale_block);
}

void scale_mod_cleanup(ubx_node_t *nd)
{
	ubx_block_unregister(nd, "oot/scale");
	ubx_type_unregister(nd, "struct scale_config");
}

UBX_MODULE_INIT(scale_mod_init)
UBX_MODULE_CLEANUP(scale_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
