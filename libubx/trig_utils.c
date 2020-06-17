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
static const char *LOG_FMT = "TSTAT: %s: cnt %" PRIu64 ", min %" PRIu64 " us, max %" PRIu64 " us, avg %" PRIu64 " us";
static const char *tstat_global_id = "#total#";

def_port_accessors(tstat, struct ubx_tstat)
def_cfg_getptr_fun(cfg_getptr_ubx_triggee, struct ubx_triggee)

/*
 * timinig statistics helpers
 */

/**
 * initialise a tstat structure
 */
void tstat_init(struct ubx_tstat *ts, const char *block_name)
{
	strncpy(ts->block_name, block_name, UBX_BLOCK_NAME_MAXLEN);
	ts->block_name[UBX_BLOCK_NAME_MAXLEN] = '\0';

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
 * tstat_write_file - write a single tstat to a file
 *
 * @b block for logging
 * @tstats
 * @profile_path
 * @return - if sucess, <0 otherwise
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
 * tstat_log - log timing statistics
 *
 * @b: block
 * @stats: ubx_tstat to log
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
static void tstats_output_throttled(struct ubx_chain *chain, uint64_t now)
{
	if (now <= chain->tstats_output_last_msg + chain->tstats_output_rate)
		return;

	if (chain->tstats_mode == TSTATS_GLOBAL) {
		write_tstat(chain->p_tstats, &chain->global_tstats);
	} else { /* mode == TSTATS_PERBLOCK */
		if (chain->tstats_output_idx < chain->triggees_len) {
			write_tstat(chain->p_tstats,
				    &chain->blk_tstats[chain->tstats_output_idx]);
		} else {
			write_tstat(chain->p_tstats, &chain->global_tstats);
		}

		chain->tstats_output_idx =
			(chain->tstats_output_idx+1) % (chain->triggees_len+1);
	}
	chain->tstats_output_last_msg = now;
}

/**
 * ubx_chain_output_tstats - output *all* tstats on the tstats_port
 *
 * @b: block, only for logging errors
 * @chain: ubx_chain to output
 */
void ubx_chain_output_tstats(ubx_block_t *b, struct ubx_chain *chain)
{
	switch (chain->tstats_mode) {
	case TSTATS_DISABLED:
		return;
	case TSTATS_PERBLOCK:
		for(int i=0; i<chain->triggees_len; i++)
			write_tstat(chain->p_tstats, &chain->blk_tstats[i]);
		/* fall through */
	case TSTATS_GLOBAL:
		write_tstat(chain->p_tstats, &chain->global_tstats);
		break;
	default:
		ubx_err(b, "invalid TSTATS_MODE %i", chain->tstats_mode);
		return;
	}
}

/**
 * trig_stats_perblock
 *
 * trigger the given chain and aquire per-block statistics
 */
static int trig_stats_perblock(struct ubx_chain *chain)
{
	int ret = 0;
	uint64_t ts_end_ns;
	struct ubx_timespec ts_start, ts_end, blk_ts_start, blk_ts_end;

	ubx_gettime(&ts_start);

	/* trigger all blocks */
	for (int i = 0; i < chain->triggees_len; i++) {

		const struct ubx_triggee *trig = &chain->triggees[i];

		ubx_gettime(&blk_ts_start);

		/* step block */
		for (int steps = 0; steps < trig->num_steps; steps++) {
			if (ubx_cblock_step(trig->b) != 0) {
				ret = -1;
				break; /* next block */
			}
		}

		ubx_gettime(&blk_ts_end);
		tstat_update(&chain->blk_tstats[i], &blk_ts_start, &blk_ts_end);
	}

	/* finalize global measurement,	output stats */
	ubx_gettime(&ts_end);
	tstat_update(&chain->global_tstats, &ts_start, &ts_end);
	ts_end_ns = ubx_ts_to_ns(&ts_end);

	if (chain->tstats_output_rate > 0 && chain->p_tstats != NULL)
		tstats_output_throttled(chain, ts_end_ns);

	return ret;
}

/**
 * trig_stats_global
 *
 * trigger the given chain and aquire (only) global statistics
 */
static int trig_stats_global(struct ubx_chain *chain)
{
	int ret = 0;
	uint64_t ts_end_ns;
	struct ubx_timespec ts_start, ts_end;

	ubx_gettime(&ts_start);

	/* trigger all blocks */
	for (int i = 0; i < chain->triggees_len; i++) {

		const struct ubx_triggee *trig = &chain->triggees[i];

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
	tstat_update(&chain->global_tstats, &ts_start, &ts_end);
	ts_end_ns = ubx_ts_to_ns(&ts_end);

	if (chain->tstats_output_rate > 0 && chain->p_tstats != NULL)
		tstats_output_throttled(chain, ts_end_ns);

	return ret;
}

/*
 * basic triggering and tstats management
 */
static int trig_stats_disabled(struct ubx_chain *chain)
{
	int ret = 0;
	/* trigger all blocks */
	for (int i = 0; i < chain->triggees_len; i++) {

		const struct ubx_triggee *trig = &chain->triggees[i];

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

/**
 * ubx_chain_trigger - trigger a ubx_chain
 *
 * trigger the given chain according to the configured tstats_mode.
 *
 * @chain: chain to trigger
 */
int ubx_chain_trigger(struct ubx_chain *chain)
{
	if (chain->tstats_skip_first > 0) {
		chain->tstats_skip_first--;
		return trig_stats_disabled(chain);
	}

	switch(chain->tstats_mode) {
	case TSTATS_DISABLED:
		return trig_stats_disabled(chain);
	case TSTATS_GLOBAL:
		return trig_stats_global(chain);
	case TSTATS_PERBLOCK:
		return trig_stats_perblock(chain);
	default:
		ERR("invalid TSTATS_MODE %i", chain->tstats_mode);
		return -1;
	}
}

int ubx_chain_init(struct ubx_chain *chain,
		   const char *list_id,
		   double tstats_output_rate)
{
	chain->tstats_output_rate = tstats_output_rate * NSEC_PER_SEC;
	chain->tstats_output_last_msg = 0;
	chain->tstats_output_idx = 0;

	list_id = (list_id != NULL) ? list_id : tstat_global_id;
	tstat_init(&chain->global_tstats, list_id);

	if (chain->tstats_mode >= 2) {
		chain->blk_tstats = realloc(
			chain->blk_tstats,
			chain->triggees_len * sizeof(struct ubx_tstat));

		if (!chain->blk_tstats)
			return EOUTOFMEM;

		for (int i = 0; i < chain->triggees_len; i++)
			tstat_init(&chain->blk_tstats[i], chain->triggees[i].b->name);
	}

	/* let num_steps default to 1 */
	for (int i = 0; i < chain->triggees_len; i++) {
		if (chain->triggees[i].num_steps == 0)
			((struct ubx_triggee*) &chain->triggees[i])->num_steps = 1;
	}

	return 0;
}

void ubx_chain_cleanup(struct ubx_chain *chain)
{
	free(chain->blk_tstats);
}

/**
 * log all tstats of the given chain
 */
void ubx_chain_tstats_log(ubx_block_t *b, struct ubx_chain *chain)
{
	switch (chain->tstats_mode) {
	case TSTATS_DISABLED:
		break;
	case TSTATS_PERBLOCK:
		for (int i = 0; i < chain->triggees_len; i++)
			tstat_log(b, &chain->blk_tstats[i]);
		/* fall through */
	case TSTATS_GLOBAL:
		tstat_log(b, &chain->global_tstats);
		break;
	default:
		ubx_err(b, "unknown tstats_mode %d", chain->tstats_mode);
	}
}

/**
 * tstats_build_filename - construct the tstats log file name
 *
 * sanitze name, append .tstats and append to profile path.
 *
 * @name base file name
 * @profile path
 * @return filename, must be freed by caller!
 */
char* tstats_build_filename(const char *name, const char *profile_path)
{
	int len, ret;
	char *n;
	char *filename = NULL;

	n = strdup(name);

	if (!n)
		goto out;

	/* sanitize filename */
	char_replace(n, '/', '-');

	/* the + 1 is for the '/' */
	len = strlen(profile_path) + 1 + strlen(n) + strlen(".tstats");

	filename = malloc(len+1);

	if (filename == NULL) {
		goto out_free;
	}

	ret = snprintf(filename, len, "%s/%s.tstats", profile_path, n);

	if (ret < 0)
		goto out_err;

	/* all good */
	goto out_free;

out_err:
	free(filename);
	filename = NULL;
out_free:
	free(n);
out:
	return filename;
}

/**
 * tstat_write_file
 *
 * write a single tstats entry to a tstats file using the base name
 * tstats->block_name
 */
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
 * ubx_chain_tstats_write
 *
 * write all tstats of the given chain to a file called
 * "<profile_path>/<chain_id>.tsats"
 *
 * @b parent block for logging
 * @chain ubx_chain to write
 * @profile_path directory into which to write stats files
 * @return 0 if success, <0 otherwise
 */
int ubx_chain_tstats_write(ubx_block_t *b,
			   struct ubx_chain *chain,
			   const char *profile_path)
{
	int ret = -1;
	char *filename = NULL;
	FILE *fp;

	if (profile_path == NULL) {
		ubx_err(b, "%s: profile_path is NULL", __func__);
		goto out;
	}

	if (chain->tstats_mode == TSTATS_DISABLED) {
		ret = 0;
		goto out;
	}

	filename = tstats_build_filename(chain->global_tstats.block_name, profile_path);

	if (filename == NULL) {
		ubx_err(b, "%s: failed to build filename for %s",
			__func__, chain->global_tstats.block_name);
		goto out;
	}

	fp = fopen(filename, "w");

	if (fp == NULL) {
		ubx_err(b, "%s: opening %s failed", __func__, filename);
		goto out_free;
	}

	fprintf(fp, "%s", FILE_HDR);

	switch (chain->tstats_mode) {
	case TSTATS_PERBLOCK:
		for (int i = 0; i < chain->triggees_len; i++)
			tstat_write(fp, &chain->blk_tstats[i]);
		/* fall through */
	case TSTATS_GLOBAL:
		tstat_write(fp, &chain->global_tstats);
		break;
	default:
		ubx_err(b, "%s: unknown tstats_mode %d",
			__func__, chain->tstats_mode);
	}

	fclose(fp);

	ubx_info(b, "wrote tstats_profile to %s", filename);
	ret = 0;

out_free:
	free(filename);
out:
	return ret;
}
