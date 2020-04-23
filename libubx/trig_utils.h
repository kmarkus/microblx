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
#include "trig_spec.h"
#include "tstat.h"

enum tstats_mode {
	TSTATS_DISABLED=0,
	TSTATS_GLOBAL,
	TSTATS_PERBLOCK
};

/* helper to retrieve config */
long cfg_getptr_trig_spec(const ubx_block_t *b,
			  const char *cfg_name,
			  const struct ubx_trig_spec **valptr);

/* tstats port read/write helpers */
long read_tstat(const ubx_port_t* p, struct ubx_tstat* val);
int write_tstat(const ubx_port_t *p, const struct ubx_tstat *val);
long read_tstat_array(const ubx_port_t* p, struct ubx_tstat* val, const int len);
int write_tstat_array(const ubx_port_t* p, const struct ubx_tstat* val, const int len);

/**
 * struct trigger_info - basic trigger information
 *
 * This data-structure holds all information require to trigger a
 * sequence of blocks and to perform timing statistics. It must be
 * initialized and cleaned up with trig_info_init and _cleanup (s.b).
 *
 * @trig_list: pointer to trig_spec array
 * @trig_list_len: length of above array
 * @tstats_mode: desired enum tstats_mode
 * @p_tstats: tstats output port (optional)
 * @global_tstats global tstats structure
 * @blk_tstats: pointer to array of size trig_list_len for per block stats
 * @tstats_output_rate:	output rate
 * @tstats_output_last_msg: timestamp of last message
 * @tstats_output_idx: index of last output sample
 */
struct trig_info {
	const struct ubx_trig_spec *trig_list;
	long trig_list_len;
	int tstats_mode;
	ubx_port_t *p_tstats;

	/* internal, initialized via trig_info_init */
	struct ubx_tstat global_tstats;
	struct ubx_tstat *blk_tstats;

	uint64_t tstats_output_rate;
	uint64_t tstats_output_last_msg;
	long tstats_output_idx;
};

/**
 * trig_info_init - initialize a trig_info structure
 *
 * initialize the given trig_info and allocate tstat buffers according
 * to the mode. It is OK to re-run this function multiple times
 * (e.g. in start), as it will resize existing buffers appropriately.
 *
 * Before initializing, make sure to set the @trig_list,
 * @trig_list_len @tstats_mode and optionally the tstats output port
 * @p_tstats.
 *
 * @param trig_inf: trig_inf to initialized
 * @param list_id: id for this trigger list (used in as name filed in
 * 	  global tstats. Can be NULL, then default is used.
 * @param tstats_output_rate: tstats output rate [sec] (0 to disable port tstats output)
 * @return 0 if OK, < 0 otherwise
 */
int trig_info_init(struct trig_info* trig_inf,
		   const char *list_id,
		   double tstats_output_rate);

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
 * construct the tstats log file name
 * @param blockname of trigger block
 * @param profile path
 * @return filename (must be freed by called!)
 */
char* tstats_build_filename(const char *blockname,
			    const char *profile_path);

/**
 * write all tstats to file named "block.tstats"
 * @param b parent block for logging
 * @param trig_inf trigger info struct
 * @param profile_path directory into which to write stats files
 * @return 0 if success, <0 otherwise
 */
int trig_info_tstats_write(ubx_block_t *b,
			   struct trig_info *trig_inf,
			   const char *profile_path);


/**
 * write a single tstat to a file
 * @param b block for logging
 * @param tstats
 * @param profile_path
 * @return - if sucess, <0 otherwise
 */
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
