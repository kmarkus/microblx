/*
 * Performance profiling functions
 *
 * SPDX-License-Identifier: MPL-2.0
 */


#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include "trig_utils.h"

#define FILE_HDR "block, cnt, min, max, avg\n"
#define FILE_FMT "%s, %lu, %11.9f, %11.9f, %11.9f\n"
#define LOG_FMT "TSTAT: %s: cnt %lu, min %11.9f, max %11.9f, avg %11.9f"
static const char *tstat_global_id = "#total#";

def_port_accessors(tstat, struct ubx_tstat)
def_cfg_getptr_fun(cfg_getptr_trig_spec, struct ubx_trig_spec)

/*
 * timinig statistics helpers
 */

/**
 * initialise a tstat structure
 */
void tstat_init(struct ubx_tstat *ts, const char *block_name)
{
	strncpy(ts->block_name, block_name, BLOCK_NAME_MAXLEN);
	ts->block_name[BLOCK_NAME_MAXLEN] = '\0';

	ts->min.sec = LONG_MAX;
	ts->min.nsec = LONG_MAX;

	ts->max.sec = 0;
	ts->max.nsec = 0;

	ts->total.sec = 0;
	ts->total.nsec = 0;
}

/**
 * update timing statistics with new information
 */
void tstat_update(struct ubx_tstat *stats,
		  struct ubx_timespec *start,
		  struct ubx_timespec *end)
{
	struct ubx_timespec dur;

	ubx_ts_sub(end, start, &dur);

	if (ubx_ts_cmp(&dur, &stats->min) == -1)
		stats->min = dur;

	if (ubx_ts_cmp(&dur, &stats->max) == 1)
		stats->max = dur;

	ubx_ts_add(&stats->total, &dur, &stats->total);
	stats->cnt++;
}

/**
 * write timing statistics to file
 * @param trig_inf struct trig_info's content to write
 * @return 0 if OK, -1 if opening or writing file failed.
 */
int tstat_write(FILE *fp, struct ubx_tstat *stats)
{
	struct ubx_timespec avg;

	if (stats->cnt > 0) {
		ubx_ts_div(&stats->total, stats->cnt, &avg);

		fprintf(fp, FILE_FMT,
			stats->block_name, stats->cnt,
			ubx_ts_to_double(&stats->min),
			ubx_ts_to_double(&stats->max),
			ubx_ts_to_double(&avg));
	} else {
		fprintf(fp, "%s: cnt: 0 - no stats aquired\n", stats->block_name);
	}

	return 0;
}

/**
 * log timing statistics
 */
void tstat_log(const ubx_block_t *b, const struct ubx_tstat *stats)
{
	struct ubx_timespec avg;

	if (stats->cnt == 0) {
		ubx_info(b, "%s: cnt: 0 - no statistics aquired",
			 stats->block_name);
		return;
	}

	ubx_ts_div(&stats->total, stats->cnt, &avg);

	ubx_info(b, LOG_FMT,
		 stats->block_name, stats->cnt,
		 ubx_ts_to_double(&stats->min),
		 ubx_ts_to_double(&stats->max),
		 ubx_ts_to_double(&avg));
}

/* helpers for throttled port output or logging */
static void tstats_output_throttled(struct trig_info *trig_inf, uint64_t now)
{
	if (now <= trig_inf->tstats_output_last_msg + trig_inf->tstats_output_rate)
		return;

	if (trig_inf->tstats_mode == TSTATS_GLOBAL) {
		write_tstat(trig_inf->p_tstats, &trig_inf->global_tstats);
	} else { /* mode == TSTATS_PERBLOCK */
		if (trig_inf->tstats_output_idx < trig_inf->trig_list_len) {
			write_tstat(trig_inf->p_tstats,
				    &trig_inf->blk_tstats[trig_inf->tstats_output_idx]);
		} else {
			write_tstat(trig_inf->p_tstats, &trig_inf->global_tstats);
		}

		trig_inf->tstats_output_idx =
			(trig_inf->tstats_output_idx+1) % (trig_inf->trig_list_len+1);
	}
	trig_inf->tstats_output_last_msg = now;
}

/*
 * basic triggering and tstats management
 */
int do_trigger(struct trig_info *trig_inf)
{
	int ret = 0;
	unsigned int steps;
	uint64_t ts_end_ns;
	struct ubx_timespec ts_start, ts_end, blk_ts_start, blk_ts_end;


	/* start of global measurement */
	if (trig_inf->tstats_mode > 0)
		ubx_gettime(&ts_start);

	/* trigger all blocks */
	for (int i = 0; i < trig_inf->trig_list_len; i++) {

		if (trig_inf->tstats_mode == 2)
			ubx_gettime(&blk_ts_start);

		/* step block */
		for (steps = 0; steps < trig_inf->trig_list[i].num_steps; steps++) {
			if (ubx_cblock_step(trig_inf->trig_list[i].b) != 0) {
				ret = -1;
				break; /* next block */
			}
		}

		if (trig_inf->tstats_mode == 2) {
			ubx_gettime(&blk_ts_end);
			tstat_update(&trig_inf->blk_tstats[i], &blk_ts_start, &blk_ts_end);
		}
	}


	if (trig_inf->tstats_mode == TSTATS_DISABLED)
		goto out;

	/* finalize global measurement,	output stats */
	ubx_gettime(&ts_end);
	tstat_update(&trig_inf->global_tstats, &ts_start, &ts_end);
	ts_end_ns = ubx_ts_to_ns(&ts_end);

	if (trig_inf->tstats_output_rate > 0 && trig_inf->p_tstats != NULL)
		tstats_output_throttled(trig_inf, ts_end_ns);

out:
	return ret;
}

int trig_info_init(struct trig_info *trig_inf,
		   const char *list_id,
		   double tstats_output_rate)
{
	trig_inf->tstats_output_rate = tstats_output_rate * NSEC_PER_SEC;
	trig_inf->tstats_output_last_msg = 0;
	trig_inf->tstats_output_idx = 0;

	list_id = (list_id != NULL) ? list_id : tstat_global_id;
	tstat_init(&trig_inf->global_tstats, list_id);

	if (trig_inf->tstats_mode >= 2) {
		trig_inf->blk_tstats = realloc(
			trig_inf->blk_tstats,
			trig_inf->trig_list_len * sizeof(struct ubx_tstat));

		if (!trig_inf->blk_tstats)
			return EOUTOFMEM;

		for (int i = 0; i < trig_inf->trig_list_len; i++)
			tstat_init(&trig_inf->blk_tstats[i], trig_inf->trig_list[i].b->name);
	}
	return 0;
}

void trig_info_cleanup(struct trig_info *trig_inf)
{
	free(trig_inf->blk_tstats);
}

/**
 * log all tstats
 */
void trig_info_tstats_log(ubx_block_t *b, struct trig_info *trig_inf)
{
	switch (trig_inf->tstats_mode) {
	case TSTATS_DISABLED:
		break;
	case TSTATS_PERBLOCK:
		for (int i = 0; i < trig_inf->trig_list_len; i++)
			tstat_log(b, &trig_inf->blk_tstats[i]);
		/* fall through */
	case TSTATS_GLOBAL:
		tstat_log(b, &trig_inf->global_tstats);
		break;
	default:
		ubx_err(b, "unknown tstats_mode %d", trig_inf->tstats_mode);
	}
}

/**
 * write all stats to file
 */
int trig_info_tstats_write(ubx_block_t *b,
			   struct trig_info *trig_inf,
			   const char *profile_path)
{
	int ret=0, len;
	char *blockname=NULL, *filename=NULL;
	FILE *fp;

	if (profile_path == NULL)
		goto out;

	if (trig_inf->tstats_mode == TSTATS_DISABLED)
		goto out;

	blockname = strdup(trig_inf->global_tstats.block_name);

	if (!blockname) {
		ubx_err(b, "%s: EOUTOFMEM", __func__);
		ret = EOUTOFMEM;
		goto out;
	}

	/* sanitize filename */
	char_replace(blockname,	'/', '-');

	/* the + 1 is for the '/' */
	len = strlen(profile_path) + 1 + strlen(blockname) + strlen(".tstats");

	filename = malloc(len+1);

	if (!filename) {
		ubx_err(b, "%s: EOUTOFMEM 2", __func__);
		ret = EOUTOFMEM;
		goto out_free;
	}

	ret = snprintf(filename, len, "%s/%s.tstats",
		       profile_path, blockname);

	if (ret < 0) {
		ubx_err(b, "%s: failed to build filepath", __func__);
		goto out_free;
	}

	fp = fopen(filename, "w");

	if (fp == NULL) {
		ubx_err(b, "%s: opening %s failed", __func__, filename);
		ret = -1;
		goto out_free;
	}

	fprintf(fp, FILE_HDR);

	switch (trig_inf->tstats_mode) {
	case TSTATS_PERBLOCK:
		for (int i = 0; i < trig_inf->trig_list_len; i++)
			tstat_write(fp, &trig_inf->blk_tstats[i]);
		/* fall through */
	case TSTATS_GLOBAL:
		tstat_write(fp, &trig_inf->global_tstats);
		break;
	default:
		ubx_err(b, "%s: unknown tstats_mode %d",
			__func__, trig_inf->tstats_mode);
	}

	fclose(fp);

	ubx_info(b, "wrote tstats_profile to %s", filename);
	ret = 0;


out_free:
	free(blockname);
	free(filename);
out:
	return ret;
}
