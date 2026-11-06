/*
 * lfbm_shm.h - POSIX shared memory lifecycle for lfbm rings
 *
 * Copyright (C) 2026 Markus Klotzbuecher <mk@mkio.de>
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef _LFBM_SHM_H_
#define _LFBM_SHM_H_

#include "lfbm.h"
#include "lfb_shm.h"

/**
 * @brief create or attach to a ring in a shm segment, see lfb_shm_join
 *
 * @param s handle to initialize (caller-owned memory)
 * @param name shm segment name
 * @param payload_sz frame size, a multiple of 8
 * @param depth number of frames, a power of two
 * @param user_sz size of the caller-owned user area (0 for none)
 * @return 1 if created (lfb_shm_publish it!), 0 if joined, negative
 *         errno on failure as per lfb_shm_join
 */
static inline int lfbm_shm_join(lfb_shm_t *s, const char *name,
				uint32_t payload_sz, lfb_word_t depth,
				uint32_t user_sz)
{
	if (!lfbm_geometry_ok(payload_sz, depth))
		return -EINVAL;

	/* seq must be 0 in unused slots: a segment created here is fresh */
	return lfb_shm_join_mode(s, name, LFBM_SLOT_SZ(payload_sz), depth,
				 user_sz, LFB_MODE_MP);
}

/**
 * @brief open a ring in a shm segment read-only, see lfb_shm_open
 *
 * @param s handle to initialize (caller-owned memory)
 * @param name shm segment name
 * @return 0 on success, negative errno as per lfb_shm_open; -EPROTO
 *         also if the segment holds a plain lfb buffer
 */
static inline int lfbm_shm_open(lfb_shm_t *s, const char *name)
{
	return lfb_shm_open_mode(s, name, LFB_MODE_MP);
}

#endif /* _LFBM_SHM_H_ */
