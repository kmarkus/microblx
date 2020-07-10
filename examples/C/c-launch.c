/*
 * This minimal example shows how to startup a microblx application
 * without the scripting layer.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <ubx/ubx.h>

#define WEBIF_PORT	"8810"

int main(int argc, char **argv)
{
	ubx_node_t nd;
	ubx_block_t *webif;
	int ret = EXIT_FAILURE;

	/* initalize the node */
	ubx_node_init(&nd, "c-launch", 0);

	/* load the standard types */
	if(ubx_module_load(&nd, "/usr/local/lib/ubx/0.9/stdtypes.so") != 0)
		goto out;

	/* load the web-interface block */
	if(ubx_module_load(&nd, "/usr/local/lib/ubx/0.9/webif.so") != 0)
		goto out;

	/* create a webserver block */
	if((webif = ubx_block_create(&nd, "webif/webif", "webif"))==NULL)
		goto out;

	/* webif port config */
	if (cfg_set_char(webif, "port", WEBIF_PORT, strlen(WEBIF_PORT))) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd,__func__,  "failed to configure port_vel");
		goto out;
	}

	/* init and start the block */
	if(ubx_block_init(webif) != 0) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd, "c-launch", "failed to init webif");
		goto out;
	}

	if(ubx_block_start(webif) != 0) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd, "c-launch", "failed to start webif");
		goto out;
	}

	printf("started system,	webif @ http://localhost:%s\n", WEBIF_PORT);

	/* wait for SIGINT (ctrl + c) */
	ubx_wait_sigint(UINT_MAX);
	printf("shutting down\n");
	ret = EXIT_SUCCESS;
 out:
	/* this cleans up all blocks and unloads all modules */
	ubx_node_rm(&nd);
	exit(ret);
}
