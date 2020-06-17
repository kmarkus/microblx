/*
 * Generic trigger implementation.
 *
 * Copyright (C) 2019 Hamish Guthrie <hamish.guthrie@kistler.com>
 * Copyright (C) 2019-2020 Markus Klotzbuecher <mk@mkio.de>
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef TRIG_UTILS_H
#define TRIG_UTILS_H

#include "ubx.h"
#include "triggee.h"
#include "tstat.h"

enum tstats_mode {
	TSTATS_DISABLED=0,
	TSTATS_GLOBAL,
	TSTATS_PERBLOCK
};

/* helper to retrieve config */
long cfg_getptr_ubx_triggee(const ubx_block_t *b,
			  const char *cfg_name,
			  const struct ubx_triggee **valptr);

/* tstats port read/write helpers */
long read_tstat(const ubx_port_t* p, struct ubx_tstat* val);
int write_tstat(const ubx_port_t *p, const struct ubx_tstat *val);
long read_tstat_array(const ubx_port_t* p, struct ubx_tstat* val, const long len);
int write_tstat_array(const ubx_port_t* p, const struct ubx_tstat* val, const long len);

/**
 * struct ubx_chain
 *
 * This data-structure holds all information require to trigger a
 * sequential list of blocks and aquire timing statistics. It must be
 * initialized and cleaned up with ubx_chain_init and _cleanup (s.b).
 *
 * @triggees: pointer to an ubx_triggee array, i.e. the array of blocks to trigger
 * @triggees_len: length of the above array
 * @tstats_mode: desired enum tstats_mode
 * @tstats_skip_first skip this many steps before starting to acquire stats
 * @p_tstats: tstats output port (optional)
 * @global_tstats global tstats structure
 * @blk_tstats: pointer to array of size trig_list_len for per block stats
 * @tstats_output_rate:	output rate
 * @tstats_output_last_msg: timestamp of last message
 * @tstats_output_idx: index of last output sample
 */
struct ubx_chain {
	/* public fields to be configured directly */
	const struct ubx_triggee *triggees;
	long triggees_len;
	int tstats_mode;
	unsigned int tstats_skip_first;
	ubx_port_t *p_tstats;

	/* internal, initialized via ubx_chain_init */
	struct ubx_tstat global_tstats;
	struct ubx_tstat *blk_tstats;

	uint64_t tstats_output_rate;
	uint64_t tstats_output_last_msg;
	long tstats_output_idx;
};

/**
 * ubx_chain_init - initialize a ubx_chain structure
 *
 * initialize the given chain and allocate tstat buffers according to
 * the mode. It is OK to re-run this function multiple times (e.g. in
 * start), as it will resize existing buffers appropriately.
 *
 * Before initializing, make sure to set the @triggees, @triggees_len
 * @tstats_mode and optionally the tstats output port @p_tstats.
 *
 * @chain: chain to initialized
 * @chain_id: id for this chain (used as id of global stats and
 *            as a file name for statistics files. Can be NULL, then default is used.
 * @tstats_output_rate: tstats output rate [sec] (0 to disable tstats output on port)
 * @return 0 if OK, < 0 otherwise
 */
int ubx_chain_init(struct ubx_chain* chain,
		   const char *chain_id,
		   double tstats_output_rate);

/**
 * chain_cleanup - release allocated resources
 *
 * this free resouces allocated by ubx_chain_init such as tstat
 * buffers.
 *
 * @chain: ubx_chain to cleanup
 */
void ubx_chain_cleanup(struct ubx_chain* chain);

/**
 * ubx_do_trigger - trigger blocks described by a ubx_triggee list
 *
 * @spec: trigger specification
 * @return 0 if successfull, <0 otherwise
 */
int ubx_chain_trigger(struct ubx_chain* chain);

/**
 * ubx_chain_tstats_log - log all tstats
 *
 * @b ubx_block in whose context to log
 * @chain trigger info to log
 */
void ubx_chain_tstats_log(ubx_block_t *b, struct ubx_chain *chain);

/**
 * ubx_chain_output_tstats - write _all_ current stats to the tstats port
 *
 * @b: block
 * @chain trigger info to log
 */
void ubx_chain_output_tstats(ubx_block_t *b, struct ubx_chain *chain);

char* tstats_build_filename(const char *blockname,
			    const char *profile_path);

int ubx_chain_tstats_write(ubx_block_t *b,
			   struct ubx_chain *chain,
			   const char *profile_path);


int tstat_write_file(ubx_block_t *b,
		     struct ubx_tstat *tstats,
		     const char *profile_path);

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
