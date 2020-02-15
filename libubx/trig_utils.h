/*
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef TRIG_UTILS_H
#define TRIG_UTILS_H

#include "ubx.h"
#include "trig_spec.h"
#include "../std_types/stdtypes/types/tstat.h"

enum tstats_mode {
	TSTATS_DISABLED=0,
	TSTATS_GLOBAL,
	TSTATS_PERBLOCK
};

/**
 * struct trigger_info - basic trigger information
 * @trig_config: trigger specification array
 * @trig_list_len: length of above array
 * @tstats_mode: enum tstats_mode
 * @profile_path: file to write with profile data
 * @global_tstats
 * @blk_tstats:
 * @port_tstats: tstats port
 */
struct trig_info {
	struct ubx_trig_spec *trig_list;
	unsigned int trig_list_len;

	int tstats_mode;
	const char *profile_path;

	struct ubx_tstat global_tstats;
	struct ubx_tstat *blk_tstats;

	ubx_port_t *p_tstats;
};

/**
 * initialize the given trig info and allocate tstat buffers
 * requires that the fields trig_spec and trig_spec_len are correctly
 * populated.
 * @param inf
 * @return 0 if OK, < 0 otherwise
 */
int trig_info_init(struct trig_info* trig_inf);

/**
 * initialize the given trig info and allocate tstat buffers
 * requires that the fields trig_spec and trig_spec_len are correctly
 * populated.
 * @param inf strig_info
 */
void trig_info_cleanup(struct trig_info* trig_inf);

/**
 * ubx_do_trigger() - trigger blocks described by a trig_spec list
 * @spec: trigger specification
 * @return 0 if successfull, <0 otherwise
 */
int do_trigger(struct trig_info* trig_inf);

/**
 * log all tstats
 */
void trig_info_tstats_log(ubx_block_t *b, struct trig_info *trig_inf);


int trig_info_tstats_write(struct trig_info *trig_inf);

/* timing statistics related */
void tstat_init(struct ubx_tstat *ts, const char *block_name);
void tstat_update(struct ubx_tstat *stats,
		  struct ubx_timespec *start,
		  struct ubx_timespec *end);
void tstat_print(const char *profile_path, struct ubx_tstat *stats);

void tstat_log(const ubx_block_t *b, const struct ubx_tstat *stats);


#endif /* TRIG_UTILS_H */
