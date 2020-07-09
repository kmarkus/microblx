/*
 * A simple activity-less trigger block
 */

#undef UBX_DEBUG

#include <stdio.h>
#include <stdlib.h>

#include "ubx.h"
#include "trig_utils.h"
#include "common.h"

/* trig metadata */
char trig_meta[] =
	"{ doc='simple, activity-less trigger',"
	"  realtime=true,"
	"}";

ubx_proto_port_t trig_ports[] = {
	{ .name = "active_chain", .in_type_name = "int", .doc = "switch the active trigger chain" },
	{ .name = "tstats", .out_type_name = "struct ubx_tstat", .doc = "timing statistics (if enabled)"},
	{ 0 },
};

/* configuration */
ubx_proto_config_t trig_config[] = {
	{ .name = "num_chains", .type_name = "int", .max = 1, .doc = "number of trigger chains. def: 1" },
	{ .name = "tstats_mode", .type_name = "int", .max = 1, .doc = "0: off (def), 1: global only, 2: per block", },
	{ .name = "tstats_profile_path", .type_name = "char", .doc = "directory to write the timing stats file to" },
	{ .name = "tstats_output_rate", .type_name = "double", .max = 1, .doc = "throttle output on tstats port" },
	{ .name = "tstats_skip_first", .type_name = "int", .max=1, .doc = "skip N steps before acquiring stats" },
	{ .name = "loglevel", .type_name = "int" },
	{ 0 },
};

/**
 * struct block_info - block state of the trig block
 * @num_chains: number of chains that are configured
 * @chains: pointer to array of struct ubx_chain of size num_chains
 * @actchain: active chain
 * @p_actchain: pointer to active_chain port
 */
struct block_info {
	struct ubx_chain *chains;
	int num_chains;

	int actchain;
	ubx_port_t *p_actchain;
};


/* step */
void trig_step(ubx_block_t *b)
{
	struct block_info *inf = (struct block_info *)b->private_data;

	common_read_actchain(b, inf->p_actchain, inf->num_chains, &inf->actchain);

	if (ubx_chain_trigger(&inf->chains[inf->actchain]) != 0)
		ubx_err(b, "ubx_chain_trigger failed for chain%i", inf->actchain);
}

/* init */
int trig_init(ubx_block_t *b)
{
	b->private_data = calloc(1, sizeof(struct block_info));

	if (b->private_data == NULL) {
		ubx_err(b, "failed to alloc ubx_chain");
		return EOUTOFMEM;
	}

	struct block_info *inf = (struct block_info *)b->private_data;

	inf->num_chains = common_init_chains(b, &inf->chains);

	if (inf->num_chains <= 0)
		return -1;

	return 0;
}

int trig_start(ubx_block_t *b)
{
	struct block_info *inf = (struct block_info *)b->private_data;

	/* cache active chain port */
	inf->p_actchain = ubx_port_get(b, "active_chain");
	assert(inf->p_actchain != NULL);

	return common_config_chains(b, inf->chains, inf->num_chains);
}

void trig_stop(ubx_block_t *b)
{
	struct block_info *inf = (struct block_info *)b->private_data;
	common_output_stats(b, inf->chains, inf->num_chains);
	common_log_stats(b, inf->chains, inf->num_chains);
	common_write_stats(b, inf->chains, inf->num_chains);

	common_unconfig(inf->chains, inf->num_chains);
}

void trig_cleanup(ubx_block_t *b)
{
	struct block_info *inf = (struct block_info *)b->private_data;
	common_cleanup(b, &inf->chains);
	free(b->private_data);
}


ubx_proto_block_t trig_comp = {
	.name = "ubx/trig",
	.type = BLOCK_TYPE_COMPUTATION,
	.attrs = BLOCK_ATTR_TRIGGER,
	.meta_data = trig_meta,
	.configs = trig_config,
	.ports = trig_ports,

	.init = trig_init,
	.start = trig_start,
	.stop = trig_stop,
	.cleanup = trig_cleanup,
	.step = trig_step
};

int trig_mod_init(ubx_node_t *nd)
{
	return ubx_block_register(nd, &trig_comp);
}

void trig_mod_cleanup(ubx_node_t *nd)
{
	ubx_block_unregister(nd, "ubx/trig");
}

UBX_MODULE_INIT(trig_mod_init)
UBX_MODULE_CLEANUP(trig_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
