/*
 * This file was transplanted with slight modifications from Linux sources
 * (fs/cifs/md5.h) into U-Boot by Bartlomiej Sieka <tur@semihalf.com>.
 *
 * Transplantation into microblx by 
 * Markus Klotzbuecher <markus.klotzbuecher@mech.kuleuven.be>
 */

#include <stdint.h>
#include <string.h>

#ifndef _MD5_H
#define _MD5_H

struct MD5Context {
	uint32_t buf[4];
	uint32_t bits[2];
	union {
		unsigned char in[64];
		uint32_t in32[16];
	};
};

/*
 * Calculate and store in 'output' the MD5 digest of 'len' bytes at
 * 'input'. 'output' must have enough space to hold 16 bytes.
 */
void md5 (const unsigned char *input, int len, unsigned char output[16]);

#endif /* _MD5_H */
