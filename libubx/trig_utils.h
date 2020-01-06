/*
 * SPDX-License-Identifier: MPL-2.0
 */

#include "ubx.h"
#include "tstat.h"
#include "trig_spec.h"


/* timing statistics related */
void tstat_init(struct ubx_tstat* ts, const char *block_name);
void tstat_update(struct ubx_tstat *stats,
		  struct ubx_timespec *start,
		  struct ubx_timespec *end);
void tstat_print(const char *profile_path, struct ubx_tstat *stats);
