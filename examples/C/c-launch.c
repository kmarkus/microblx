/*
 * This minimal example shows how to startup a microblx application
 * without the scripting layer.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <ubx/ubx.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	ubx_node_t nd;
	ubx_block_t *rand1;
	int timeout, ret = EXIT_FAILURE;

	timeout = (argc > 1) ? atoi(argv[1]) : UINT_MAX;

	/* initalize the node */
	ubx_node_init(&nd, "c-launch", 0);

	/* load the standard types */
	if(ubx_module_load(&nd, "/usr/local/lib/ubx/0.9/stdtypes.so") != 0)
		goto out;

	/* load the rand_double block */
	if(ubx_module_load(&nd, "/usr/local/lib/ubx/0.9/rand_double.so") != 0)
		goto out;

	/* create a rand_double block */
	if((rand1 = ubx_block_create(&nd, "ubx/rand_double", "rand1"))==NULL)
		goto out;

	/* init and start the block */
	if(ubx_block_init(rand1) != 0) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd, "c-launch", "failed to init rand1");
		goto out;
	}

	if(ubx_block_start(rand1) != 0) {
		ubx_log(UBX_LOGLEVEL_ERR, &nd, "c-launch", "failed to start rand1");
		goto out;
	}

	printf("started system\n");

	/* wait for SIGINT (ctrl + c) */
	ubx_wait_sigint(timeout);
	printf("shutting down\n");
	ret = EXIT_SUCCESS;
 out:
	/* this cleans up all blocks and unloads all modules */
	ubx_node_rm(&nd);
	exit(ret);
}
