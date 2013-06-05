/*
 * An interaction block that hexdumps the data to stdout.
 */

#define DEBUG 1

#include <stdio.h>
#include <stdlib.h>

#include "u5c.h"

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

static void hexdump_write(u5c_block_t *i, u5c_data_t* data) {
	/* const char* typename = get_typename(data); */
	/* printf("hexdumping data of type %s", (typename!=NULL) ? typename : "unknown"); */
	/* hexdump(data->data, data_len(data), 16); */
	hexdump(data->data, sizeof(unsigned int), 16);
}

/* put everything together */
u5c_block_t hexdump_comp = {
	.name = "hexdump",
	.type = BLOCK_TYPE_INTERACTION,
	.meta_data = hexdumpmeta,
	
	/* ops */
	.write=hexdump_write,
};

static int hexdump_mod_init(u5c_node_info_t* ni)
{
	DBG(" ");	
	return u5c_block_register(ni, &hexdump_comp);
}

static void hexdump_mod_cleanup(u5c_node_info_t *ni)
{
	DBG(" ");
	u5c_block_unregister(ni, BLOCK_TYPE_INTERACTION, "hexdump");
}

module_init(hexdump_mod_init)
module_cleanup(hexdump_mod_cleanup)
