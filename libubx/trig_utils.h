/*
 * Generic trigger implementation.
 *
 * Copyright (C) 2019 Hamish Guthrie <hamish.guthrie@kistler.com>
 * Copyright (C) 2020 Markus Klotzbuecher <mk@mkio.de>
 *
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
 *
 * This data-structure holds all information require to trigger a
 * sequence of blocks and to perform timing statistics. It must be
 * initialized and cleaned up with trig_info_init and _cleanup (s.b).
 *
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
 * trig_info_init - initialze trig_info structure
 *
 * initialize the given trig_info and allocate tstat buffers. This
 * function requires that the fields trig_spec and trig_spec_len are
 * correctly set. It is OK to re-run this function multiple times
 * (e.g. in start), as it will resize existing buffers appropriately.
 *
 * @param inf
 * @return 0 if OK, < 0 otherwise
 */
int trig_info_init(struct trig_info* trig_inf);

/**
 * trig_inf_cleanup - release allocated resources
 *
 * this free resouces allocated by trig_info_init such as tstat
 * buffers.
 *
 * @param inf trig_info
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
 * @param b ubx_block in whose context to log
 * @param trig_inf trigger info	to log
 */
void trig_info_tstats_log(ubx_block_t *b, struct trig_info *trig_inf);


/**
 * write all tstats to file "profile_path"
 * @param trig_inf trigger info struct
 * @return 0 if success, <0 otherwise
 */
int trig_info_tstats_write(ubx_block_t *b, struct trig_info *trig_inf);

/*
 * helpers to manager timing statistics
 */
void tstat_init(struct ubx_tstat *ts, const char *block_name);
void tstat_update(struct ubx_tstat *stats,
		  struct ubx_timespec *start,
		  struct ubx_timespec *end);
void tstat_print(const char *profile_path, struct ubx_tstat *stats);

void tstat_log(const ubx_block_t *b, const struct ubx_tstat *stats);


#endif /* TRIG_UTILS_H */
