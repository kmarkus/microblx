/*
 * Performance profiling functions
 *
 * SPDX-License-Identifier: MPL-2.0
 */


#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include "trig_utils.h"

#define LOG_FMT "cnt: %lu, min: %11.9f, max: %11.9f, avg: %11.9f"
static const char *tstat_global_id = "##total##";

def_write_fun(write_tstat, struct ubx_tstat)

/*
 * timinig statistics helpers
 */

/**
 * initialise a tstat structure
 */
void tstat_init(struct ubx_tstat *ts, const char *block_name)
{
	strncpy(ts->block_name, block_name, BLOCK_NAME_MAXLEN);

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

		fprintf(fp,
			"%s: " LOG_FMT "\n",
			stats->block_name, stats->cnt,
			ubx_ts_to_double(&stats->min),
			ubx_ts_to_double(&stats->max),
			ubx_ts_to_double(&avg));
	} else {
		fprintf(fp, "%s: cnt: 0 - no stats aquired", stats->block_name);
	}

	fclose(fp);
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

	ubx_info(b, "%s: " LOG_FMT,
		 stats->block_name, stats->cnt,
		 ubx_ts_to_double(&stats->min),
		 ubx_ts_to_double(&stats->max),
		 ubx_ts_to_double(&avg));
}

/*
 * basic triggering and tstats management
 */

int do_trigger(struct trig_info* trig_inf)
{
	int ret = -1;
	unsigned int i, steps;

	struct ubx_timespec ts_start, ts_end, blk_ts_start, blk_ts_end;


	if (trig_inf->tstats_mode)
		ubx_gettime(&ts_start);

	for (i = 0; i < trig_inf->trig_list_len; i++) {

		if (trig_inf->tstats_mode)
			ubx_gettime(&blk_ts_start);

		for (steps = 0; steps < trig_inf->trig_list[i].num_steps; steps++) {
			if (ubx_cblock_step(trig_inf->trig_list[i].b) != 0)
				goto out;
		}

		if (trig_inf->tstats_mode) {
			ubx_gettime(&blk_ts_end);
			tstat_update(&trig_inf->blk_tstats[i], &blk_ts_start, &blk_ts_end);
		}
	}

	if (trig_inf->tstats_mode) {
		ubx_gettime(&ts_end);
		tstat_update(&trig_inf->global_tstats, &ts_start, &ts_end);

		/* output stats */
		write_tstat(trig_inf->p_tstats, &trig_inf->global_tstats);

		for (i = 0; i < trig_inf->trig_list_len; i++)
			write_tstat(trig_inf->p_tstats, &trig_inf->blk_tstats[i]);
	}

	ret = 0;
 out:
	return ret;
}

int trig_info_init(struct trig_info* trig_inf)
{
	tstat_init(&trig_inf->global_tstats, tstat_global_id);

	trig_inf->blk_tstats = realloc(
		trig_inf->blk_tstats,
		trig_inf->trig_list_len * sizeof(struct ubx_tstat));

	if (!trig_inf->blk_tstats)
		return EOUTOFMEM;

	for (unsigned int i = 0; i < trig_inf->trig_list_len; i++) {
		printf("%s: %s\n", __FUNCTION__, trig_inf->trig_list[i].b->name);
		tstat_init(&trig_inf->blk_tstats[i], trig_inf->trig_list[i].b->name);
	}

	return 0;
}

void trig_info_cleanup(struct trig_info* trig_inf)
{
	free(trig_inf->blk_tstats);
}

/**
 * log all tstats
 */
void trig_info_tstats_log(ubx_block_t *b, struct trig_info *trig_inf)
{
	switch(trig_inf->tstats_mode) {
	case TSTATS_DISABLED:
		break;
	case TSTATS_PERBLOCK:
		for (unsigned int i=0; i<trig_inf->trig_list_len; i++)
			tstat_log(b, &trig_inf->blk_tstats[i]);
		/* fall through */
	case TSTATS_GLOBAL:
		tstat_log(b, &trig_inf->global_tstats);
		break;
	default:
		ubx_err(b, "unknown tstats_mode %d", trig_inf->tstats_mode);
	}
	return;
}

/**
 * write all stats to file
 */
int trig_info_tstats_write(struct trig_info *trig_inf)
{
	FILE *fp;

	fp = fopen(trig_inf->profile_path, "a");

	/* TODO */
	if (fp == NULL)
		return -1;

	return 0;
}
