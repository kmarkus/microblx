/*
 * pack_layout.h - frame layout of the ubx/pack shm segment
 *
 * The lfb user area of a ubx/pack segment holds a struct
 * ubx_pack_layout describing the frames. A frame is
 *
 *   uint64_t present[UBX_PACK_PRESENT_WORDS(nports)] | port 0 | port 1 | ...
 *
 * with each port at its 8-byte aligned offset. Bit i of present is set
 * if port i had new data in the step that wrote the frame.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef UBX_PACK_LAYOUT_H
#define UBX_PACK_LAYOUT_H

#include <stdint.h>

#define UBX_PACK_LAYOUT_VERSION	1
#define UBX_PACK_ID_LEN		40	/* UBX_PORT_NAME_MAXLEN + 1 */
#define UBX_PACK_TYPE_LEN	64	/* UBX_TYPE_NAME_MAXLEN + 1 */

#define UBX_PACK_PRESENT_WORDS(nports)	(((nports) + 63) / 64)

/* where the ports of a frame start */
#define UBX_PACK_FRAME_START(nports) \
	((uint64_t)UBX_PACK_PRESENT_WORDS(nports) * sizeof(uint64_t))

struct ubx_pack_layout_port {
	char id[UBX_PACK_ID_LEN];	/* port name, NUL terminated */
	char type[UBX_PACK_TYPE_LEN];	/* ubx type name, NUL terminated */
	uint32_t len;			/* array length */
	uint32_t offset;		/* byte offset in the frame */
	uint32_t size;			/* bytes: type size * len */
};

struct ubx_pack_layout {
	uint32_t version;
	uint32_t nports;
	struct ubx_pack_layout_port ports[];
};

static inline uint32_t ubx_pack_layout_size(uint32_t nports)
{
	return (uint32_t)(sizeof(struct ubx_pack_layout) +
			  nports * sizeof(struct ubx_pack_layout_port));
}

static inline int ubx_pack_present(const void *frame, uint32_t i)
{
	const uint64_t *present = (const uint64_t *)frame;

	return (present[i / 64] >> (i % 64)) & 1;
}

#endif /* UBX_PACK_LAYOUT_H */
