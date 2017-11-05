/*
 * An interaction block that hexdumps the data to stdout.
 */

/* #define DEBUG 1 */

#include <stdio.h>
#include <stdlib.h>

#include "ubx.h"

UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)

char hexdumpmeta[] =
	"{ doc='hexdump interaction',"
	"  license='MIT',"
	"  real-time=false,"
	"}";

/* hexdump a buffer */
static void hexdump(unsigned char *buf, unsigned long index, unsigned long width)
{
	unsigned long i, fill;

	for (i=0;i<index;i++) { printf("%02x ", buf[i]); } /* dump data */
	for (fill=index; fill<width; fill++) printf(" ");  /* pad on right */

	printf(": ");

	/* add ascii repesentation */
	for (i=0; i<index; i++) {
		if (buf[i] < 32) printf(".");
		else printf("%c", buf[i]);
	}
	printf("\n");
}

static void hexdump_write(ubx_block_t *i, ubx_data_t* data) {
	const char* typename = get_typename(data);
	printf("hexdump (%s): ", (typename!=NULL) ? typename : "unknown");
	hexdump(data->data, data_size(data), 16);
}

/* put everything together */
ubx_block_t hexdump_comp = {
	.name = "hexdump/hexdump",
	.type = BLOCK_TYPE_INTERACTION,
	.meta_data = hexdumpmeta,
	
	/* ops */
	.write=hexdump_write,
};

static int hexdump_mod_init(ubx_node_info_t* ni)
{
	DBG(" ");	
	return ubx_block_register(ni, &hexdump_comp);
}

static void hexdump_mod_cleanup(ubx_node_info_t *ni)
{
	DBG(" ");
	ubx_block_unregister(ni, "hexdump/hexdump");
}

UBX_MODULE_INIT(hexdump_mod_init)
UBX_MODULE_CLEANUP(hexdump_mod_cleanup)
