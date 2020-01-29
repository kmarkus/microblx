/*
 * A simple activity-less trigger block
 */

#undef UBX_DEBUG

#include <stdio.h>
#include <stdlib.h>

#include "ubx.h"
#include "trig_utils.h"

/* trig metadata */
char trig_meta[] =
	"{ doc='simple, activity-less trigger',"
	"  realtime=true,"
	"}";

ubx_port_t trig_ports[] = {
	{ .name = "tstats", .out_type_name = "struct ubx_tstat", .doc = "timing statistics (if enabled)"},
	{ NULL },
};

/* configuration */
ubx_config_t trig_config[] = {
	{ .name = "trig_blocks", .type_name = "struct ubx_trig_spec", .doc = "specification of blocks to trigger" },
	{ .name = "tstats_enabled", .type_name = "int", .doc = "enable timing statistics over all blocks", },
	{ .name = "tstats_profile_path", .type_name = "char", .doc = "file to which to write the timing statistics" },
	{ .name = "tstats_output_rate", .type_name = "unsigned int", .doc = "output tstats only on every N'th trigger (0 to disable)" },
	{ .name = "loglevel", .type_name = "int" },
	{ NULL },
};

/* instance state */
struct trig_inf {
	struct ubx_trig_spec *trig_list;
	unsigned int trig_list_len;

	/* timing statistics */
	const char *tstats_profile_path;
	int tstats_enabled;

	struct ubx_tstat *tstats;

	ubx_port_t *p_tstats;
};

def_write_fun(write_tstat, struct ubx_tstat)

/**
 * print tstats
 */
void trig_tstats_print(struct trig_inf *inf)
{
	unsigned int i;

	if (inf->tstats_enabled) {
		for (i = 0; i < inf->trig_list_len; i++)
			if (inf->trig_list[i].measure)
				tstat_print(inf->tstats_profile_path,
					    &inf->tstats[i]);
	}
}

/* trigger the configured blocks */
int do_trigger(ubx_block_t *b)
{
	struct trig_inf *inf = (struct trig_inf *) b->private_data;
	int ret = -1;
	unsigned int i, steps;
	struct ubx_timespec blk_ts_start, blk_ts_end;

	for (i = 0; i < inf->trig_list_len; i++) {
		if (inf->trig_list[i].measure)
			ubx_gettime(&blk_ts_start);

		for (steps = 0; steps < inf->trig_list[i].num_steps; steps++) {
			ubx_debug(b, "step: %s", inf->trig_list[i].b->name);
			if (ubx_cblock_step(inf->trig_list[i].b) != 0)
				goto out;

			/* per block statistics */
			if (inf->trig_list[i].measure) {
				ubx_gettime(&blk_ts_end);
				tstat_update(&inf->tstats[i],
					     &blk_ts_start,
					     &blk_ts_end);
			}
		}
	}

	/* output stats - TODO throttling */
	for (i = 0; i < inf->trig_list_len; i++) {
		if (inf->trig_list[i].measure)
			write_tstat(inf->p_tstats, &inf->tstats[i]);
	}

	ret = 0;
 out:
	return ret;
}

/* step */
void trig_step(ubx_block_t *b)
{
	if (do_trigger(b) != 0)
		ubx_err(b, "do_trigger failed");
}


/* init */
int trig_init(ubx_block_t *b)
{
	int ret = EOUTOFMEM;

	b->private_data = calloc(1, sizeof(struct trig_inf));
	if (b->private_data == NULL) {
		ubx_err(b, "failed to alloc");
		goto out;
	}

	ret = 0;
	goto out;

 out:
	return ret;
}

int trig_start(ubx_block_t *b)
{
	int ret = -1;
	struct trig_inf *inf;
	ubx_data_t *trig_list_data;
	long len;
	FILE *fp;
	const int *val;

	inf = (struct trig_inf *)b->private_data;

	trig_list_data = ubx_config_get_data(b, "trig_blocks");

	/* make a copy? */
	inf->trig_list = trig_list_data->data;
	inf->trig_list_len = trig_list_data->len;

	/* this is freed in cleanup hook */
	inf->tstats = realloc(
		inf->tstats,
		inf->trig_list_len * sizeof(struct ubx_tstat));

	if (!inf->tstats) {
		ubx_err(b, "failed to alloc blk_stats");
		goto out;
	}

	for (unsigned int i = 0; i < inf->trig_list_len; i++) {
		tstat_init(&inf->tstats[i],
			   inf->trig_list[i].b->name);
	}

	len = cfg_getptr_char(b, "tstats_profile_path",
			      &inf->tstats_profile_path);
	if (len < 0) {
		ubx_err(b, "unable to retrieve tstats_profile_path parameter");
		goto out;
	}
	/* truncate the file if it exists */
	if (len > 0) {
		fp = fopen(inf->tstats_profile_path, "w");
		if (fp)
			fclose(fp);
	}

	len = cfg_getptr_int(b, "tstats_enabled", &val);
	if (len  < 0)
		goto out;

	inf->tstats_enabled = (len > 0) ? *val : 0;

	if (inf->tstats_enabled)
		ubx_debug(b, "tstats enabled");

	inf->p_tstats = ubx_port_get(b, "tstats");

	ret = 0;

out:
	return ret;
}

void trig_stop(ubx_block_t *b)
{
	struct trig_inf *inf = (struct trig_inf *)b->private_data;

	trig_tstats_print(inf);

	free(inf->tstats);
}

void trig_cleanup(ubx_block_t *b)
{
	free(b->private_data);
}


/* put everything together */
ubx_block_t trig_comp = {
	.name = "std_triggers/trig",
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

int trig_mod_init(ubx_node_info_t *ni)
{
	return ubx_block_register(ni, &trig_comp);
}

void trig_mod_cleanup(ubx_node_info_t *ni)
{
	ubx_block_unregister(ni, "std_triggers/trig");
}

UBX_MODULE_INIT(trig_mod_init)
UBX_MODULE_CLEANUP(trig_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
