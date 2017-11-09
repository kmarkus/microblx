/* This simple example shows how to build a C-only system without
 * using the scripting layer. */

#include "ubx.h"

#define WEBIF_PORT	"33000"

int main(int argc, char **argv)
{
	int len, ret=EXIT_FAILURE;
	ubx_node_info_t ni;
	ubx_block_t *webif;
	ubx_data_t *d;

	/* initalize the node */
	ubx_node_init(&ni, "c-only");

	/* load the standard types */
	if(ubx_module_load(&ni, "std_types/stdtypes/stdtypes.so") != 0)
		goto out;

	/* load the web-interface block */
	if(ubx_module_load(&ni, "std_blocks/webif/webif.so") != 0)
		goto out;

	/* create a webserver block */
	if((webif = ubx_block_create(&ni, "webif/webif", "webif1"))==NULL)
		goto out;

	/* Configure port of webserver block
	 * this gets the ubx_data_t pointer */
	d = ubx_config_get_data(webif, "port");
	len = strlen(WEBIF_PORT)+1;

	/* resize the char array as necessary and copy the port string */
	ubx_data_resize(d, len);
	strncpy(d->data, WEBIF_PORT, len);

	/* init and start the block */
	if(ubx_block_init(webif) != 0) {
		ERR("failed to init webif");
		goto out;
	}

	if(ubx_block_start(webif) != 0) {
		ERR("failed to start webif");
		goto out;
	}

	printf("webif block lauched on port %s, hit enter to quit\n", WEBIF_PORT);
	getchar();

	ret=EXIT_SUCCESS;
 out:
	/* this cleans up all blocks and unloads all modules */
	ubx_node_cleanup(&ni);
	exit(ret);
}
