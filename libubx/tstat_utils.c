/*
 * Performance profiling functions
 */

#include "ubx.h"
#include "types/tstat.h"
/**
 * initialise a tstat structure
 */

void tstat_init(struct ubx_tstat* ts, const char *block_name)
{
	strncpy(ts->block_name, block_name, BLOCK_NAME_MAXLEN);

	ts->min.sec=LONG_MAX;
	ts->min.nsec=LONG_MAX;

	ts->max.sec=0;
	ts->max.nsec=0;

	ts->total.sec=0;
	ts->total.nsec=0;
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

	if(ubx_ts_cmp(&dur, &stats->min) == -1)
		stats->min=dur;

	if(ubx_ts_cmp(&dur, &stats->max) == 1)
		stats->max=dur;

	ubx_ts_add(&stats->total, &dur, &stats->total);
	stats->cnt++;
}

/**
 * print timing statistics
 */
void tstat_print(const char *profile_path, struct ubx_tstat *stats)
{
	FILE *fp;
	struct ubx_timespec avg;

	fp = fopen(profile_path, "a");

	if (stats->cnt > 0) {
		ubx_ts_div(&stats->total, stats->cnt, &avg);

		fprintf( fp != NULL ? fp : stderr,
			 "%s: cnt: %lu, min: %11.9f, max: %11.9f, avg: %11.9f\n",
			 stats->block_name,
			 stats->cnt,
			 ubx_ts_to_double(&stats->min),
			 ubx_ts_to_double(&stats->max),
			 ubx_ts_to_double(&avg));
	} else {
		fprintf( fp != NULL ? fp : stderr,
			 "%s: cnt is 0 - no statistics acquired\n",
			 stats->block_name);
	}

	if (fp)
		fclose(fp);
}
