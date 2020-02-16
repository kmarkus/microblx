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
	{ .name = "trig_blocks", .type_name = "struct ubx_trig_spec", .doc = "list of blocks to trigger" },
	{ .name = "tstats_mode", .type_name = "int", .doc = "0: off (def), 1: global only, 2: per block", },
	{ .name = "tstats_profile_path", .type_name = "char", .doc = "file to write timing stats to" },
	{ .name = "tstats_output_rate", .type_name = "unsigned int", .doc = "throttle output on tstats port" },
	{ .name = "loglevel", .type_name = "int" },
	{ NULL },
};


/* step */
void trig_step(ubx_block_t *b)
{
	struct trig_info *trig_inf;
	trig_inf = (struct trig_info *)b->private_data;

	if (do_trigger(trig_inf) != 0)
		ubx_err(b, "do_trigger failed");
}

/* init */
int trig_init(ubx_block_t *b)
{
	b->private_data = calloc(1, sizeof(struct trig_info));

	if (b->private_data == NULL) {
		ubx_err(b, "failed to alloc trig_info");
		return EOUTOFMEM;
	}

	return 0;
}

int trig_start(ubx_block_t *b)
{
	int ret = -1;
	const int *val;
	long len;
	FILE *fp;

	struct trig_info *trig_inf;
	struct ubx_trig_spec *trig_spec;

	trig_inf = (struct trig_info *) b->private_data;

	/* tstats_mode */
	len = cfg_getptr_int(b, "tstats_mode", &val);

	if (len < 0)
		goto out;

	trig_inf->tstats_mode = (len > 0) ? *val : 0;

	if (trig_inf->tstats_mode)
		ubx_info(b, "tstats_mode: %d", trig_inf->tstats_mode);

	/* trig_blocks */
	len = ubx_config_get_data_ptr(b, "trig_blocks", (void **)&trig_spec);

	if (len < 0)
		goto out;

	/* populate trig_info and init */
	trig_inf->trig_list = trig_spec;
	trig_inf->trig_list_len = len;
	trig_info_init(trig_inf);

	len = cfg_getptr_char(b, "tstats_profile_path", &trig_inf->profile_path);

	if (len < 0) {
		ubx_err(b, "unable to retrieve tstats_profile_path parameter");
		goto out;
	}

	/* truncate the file if it exists */
	if (len > 0) {
		fp = fopen(trig_inf->profile_path, "w");
		if (fp)
			fclose(fp);
	}

	trig_inf->p_tstats = ubx_port_get(b, "tstats");

	ret = 0;

out:
	return ret;
}

void trig_stop(ubx_block_t *b)
{
	struct trig_info *trig_inf = (struct trig_info*) b->private_data;
	trig_info_tstats_log(b, trig_inf);
	trig_info_tstats_write(b, trig_inf);
}

void trig_cleanup(ubx_block_t *b)
{
	struct trig_info *trig_inf = (struct trig_info*) b->private_data;
	trig_info_cleanup(trig_inf);
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
