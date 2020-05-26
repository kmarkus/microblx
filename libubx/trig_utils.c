/*
 * Performance profiling functions
 *
 * SPDX-License-Identifier: MPL-2.0
 */


#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <inttypes.h>
#include "trig_utils.h"


static const char *FILE_HDR = "block, cnt, min_us, max_us, avg_us\n";
static const char *FILE_FMT = "%s, %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 "\n";
static const char *LOG_FMT = "TSTAT: %s: cnt %" PRIu64 " , min %" PRIu64 " us, max %" PRIu64 " us, avg %" PRIu64 " us";
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

	ts->cnt = 0;
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
			ubx_ts_to_us(&stats->min),
			ubx_ts_to_us(&stats->max),
			ubx_ts_to_us(&avg));
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
		 ubx_ts_to_us(&stats->min),
		 ubx_ts_to_us(&stats->max),
		 ubx_ts_to_us(&avg));
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

/* output all tstats on the tstats_port */
void trig_info_tstats_output(ubx_block_t *b, struct trig_info *trig_inf)
{
	switch (trig_inf->tstats_mode) {
	case TSTATS_DISABLED:
		return;
	case TSTATS_PERBLOCK:
		for(int i=0; i<trig_inf->trig_list_len; i++)
			write_tstat(trig_inf->p_tstats, &trig_inf->blk_tstats[i]);
		/* fall through */
	case TSTATS_GLOBAL:
		write_tstat(trig_inf->p_tstats, &trig_inf->global_tstats);
		break;
	default:
		ubx_err(b, "invalid TSTATS_MODE %i", trig_inf->tstats_mode);
		return;
	}
}

/*
 * basic triggering and tstats management
 */
static int trig_stats_perblock(struct trig_info *trig_inf)
{
	int ret = 0;
	uint64_t ts_end_ns;
	struct ubx_timespec ts_start, ts_end, blk_ts_start, blk_ts_end;

	ubx_gettime(&ts_start);

	/* trigger all blocks */
	for (int i = 0; i < trig_inf->trig_list_len; i++) {

		const struct ubx_trig_spec *trig = &trig_inf->trig_list[i];

		ubx_gettime(&blk_ts_start);

		/* step block */
		for (int steps = 0; steps < trig->num_steps; steps++) {
			if (ubx_cblock_step(trig->b) != 0) {
				ret = -1;
				break; /* next block */
			}
		}

		ubx_gettime(&blk_ts_end);
		tstat_update(&trig_inf->blk_tstats[i], &blk_ts_start, &blk_ts_end);
	}

	/* finalize global measurement,	output stats */
	ubx_gettime(&ts_end);
	tstat_update(&trig_inf->global_tstats, &ts_start, &ts_end);
	ts_end_ns = ubx_ts_to_ns(&ts_end);

	if (trig_inf->tstats_output_rate > 0 && trig_inf->p_tstats != NULL)
		tstats_output_throttled(trig_inf, ts_end_ns);

	return ret;
}

static int trig_stats_global(struct trig_info *trig_inf)
{
	int ret = 0;
	uint64_t ts_end_ns;
	struct ubx_timespec ts_start, ts_end;

	ubx_gettime(&ts_start);

	/* trigger all blocks */
	for (int i = 0; i < trig_inf->trig_list_len; i++) {

		const struct ubx_trig_spec *trig = &trig_inf->trig_list[i];

		/* step block */
		for (int steps = 0; steps < trig->num_steps; steps++) {
			if (ubx_cblock_step(trig->b) != 0) {
				ret = -1;
				break; /* next block */
			}
		}
	}

	/* finalize global measurement,	output stats */
	ubx_gettime(&ts_end);
	tstat_update(&trig_inf->global_tstats, &ts_start, &ts_end);
	ts_end_ns = ubx_ts_to_ns(&ts_end);

	if (trig_inf->tstats_output_rate > 0 && trig_inf->p_tstats != NULL)
		tstats_output_throttled(trig_inf, ts_end_ns);

	return ret;
}

/*
 * basic triggering and tstats management
 */
static int trig_stats_disabled(struct trig_info *trig_inf)
{
	int ret = 0;
	/* trigger all blocks */
	for (int i = 0; i < trig_inf->trig_list_len; i++) {

		const struct ubx_trig_spec *trig = &trig_inf->trig_list[i];

		/* step block */
		for (int steps = 0; steps < trig->num_steps; steps++) {
			if (ubx_cblock_step(trig->b) != 0) {
				ret = -1;
				break; /* next block */
			}
		}
	}

	return ret;
}

int do_trigger(struct trig_info *trig_inf)
{
	if (trig_inf->tstats_skip_first > 0) {
		trig_inf->tstats_skip_first--;
		return trig_stats_disabled(trig_inf);
	}

	switch(trig_inf->tstats_mode) {
	case TSTATS_DISABLED:
		return trig_stats_disabled(trig_inf);
	case TSTATS_GLOBAL:
		return trig_stats_global(trig_inf);
	case TSTATS_PERBLOCK:
		return trig_stats_perblock(trig_inf);
	default:
		ERR("invalid TSTATS_MODE %i", trig_inf->tstats_mode);
		return -1;
	}
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

	/* let num_steps default to 1 */
	for (int i = 0; i < trig_inf->trig_list_len; i++) {
		if (trig_inf->trig_list[i].num_steps == 0)
			((struct ubx_trig_spec*) &trig_inf->trig_list[i])->num_steps = 1;
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

char* tstats_build_filename(const char *blockname, const char *profile_path)
{
	int len, ret;
	char *bn;
	char *filename = NULL;

	bn = strdup(blockname);

	if (!bn)
		goto out;

	/* sanitize filename */
	char_replace(bn, '/', '-');

	/* the + 1 is for the '/' */
	len = strlen(profile_path) + 1 + strlen(bn) + strlen(".tstats");

	filename = malloc(len+1);

	if (filename == NULL) {
		goto out_free;
	}

	ret = snprintf(filename, len, "%s/%s.tstats", profile_path, bn);

	if (ret < 0)
		goto out_err;

	/* all good */
	goto out_free;

out_err:
	free(filename);
	filename = NULL;
out_free:
	free(bn);
out:
	return filename;
}

int tstat_write_file(ubx_block_t *b,
		     struct ubx_tstat *tstats,
		     const char *profile_path)
{
	int ret=-1;
	char *filename=NULL;
	FILE *fp;

	if (profile_path == NULL) {
		ubx_err(b, "%s: profile_path is NULL", __func__);
		goto out;
	}

	filename = tstats_build_filename(tstats->block_name, profile_path);

	if (filename == NULL) {
		ubx_err(b, "%s: failed to build filename for %s",
			__func__, tstats->block_name);
		goto out;
	}

	fp = fopen(filename, "w");

	if (fp == NULL) {
		ubx_err(b, "%s: opening %s failed", __func__, filename);
		goto out_free;
	}

	fprintf(fp, "%s", FILE_HDR);

	tstat_write(fp, tstats);

	fclose(fp);

	ubx_info(b, "wrote tstats_profile to %s", filename);
	ret = 0;

out_free:
	free(filename);
out:
	return ret;
}

/**
 * write all stats to file
 */
int trig_info_tstats_write(ubx_block_t *b,
			   struct trig_info *trig_inf,
			   const char *profile_path)
{
	int ret=-1;
	char *filename=NULL;
	FILE *fp;

	if (profile_path == NULL) {
		ubx_err(b, "%s: profile_path is NULL", __func__);
		goto out;
	}

	if (trig_inf->tstats_mode == TSTATS_DISABLED) {
		ret = 0;
		goto out;
	}

	filename = tstats_build_filename(trig_inf->global_tstats.block_name, profile_path);

	if (filename == NULL) {
		ubx_err(b, "%s: failed to build filename for %s",
			__func__, trig_inf->global_tstats.block_name);
		goto out;
	}

	fp = fopen(filename, "w");

	if (fp == NULL) {
		ubx_err(b, "%s: opening %s failed", __func__, filename);
		goto out_free;
	}

	fprintf(fp, "%s", FILE_HDR);

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
	free(filename);
out:
	return ret;
}
