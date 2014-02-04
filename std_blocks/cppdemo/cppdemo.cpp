/*
 * A demo C++ block
 */

#define DEBUG 1

#include <iostream>
using namespace std;

#include "ubx.h"
#include <cstring>

/* function block meta-data
 * used by higher level functions.
 */
char cppdemo_meta[] =
	"{ doc='A cppdemo number generator function block',"
	"  real-time=true,"
	"}";

/* configuration
 * upon cloning the following happens:
 *   - value.type is resolved
 *   - value.data will point to a buffer of size value.len*value.type->size
 *
 * if an array is required, then .value = { .len=<LENGTH> } can be used.
 */
ubx_config_t cppdemo_config[] = {
	{ .name="test_conf", .type_name = "double" },
	{ NULL },
};


ubx_port_t cppdemo_ports[] = {
	{ .name="foo", .attrs=PORT_DIR_IN, .in_type_name="unsigned int" },
	{ .name="bar", .attrs=PORT_DIR_OUT, .out_type_name="unsigned int" },
	{ NULL },
};

static int cppdemo_init(ubx_block_t *c)
{
	cout << "cppdemo_init: hi from " << c->name << endl;
	return 0;
}


static void cppdemo_cleanup(ubx_block_t *c)
{
	cout << "cppdemo_cleanup: hi from " << c->name << endl;
}

static int cppdemo_start(ubx_block_t *c)
{
	cout << "cppdemo_start: hi from " << c->name << endl;
	return 0; /* Ok */
}

static void cppdemo_step(ubx_block_t *c) {
	cout << "cppdemo_step: hi from " << c->name << endl;
}


/* put everything together */
ubx_block_t cppdemo_comp = {
	.name = "cppdemo/cppdemo",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = cppdemo_meta,
	.configs = cppdemo_config,
	.ports = cppdemo_ports,

	/* ops */
	.init = cppdemo_init,
	.start = cppdemo_start,
	.step = cppdemo_step,
	.cleanup = cppdemo_cleanup,
};

static int cppdemo_init(ubx_node_info_t* ni)
{
	DBG(" ");
	return ubx_block_register(ni, &cppdemo_comp);
}

static void cppdemo_cleanup(ubx_node_info_t *ni)
{
	DBG(" ");
	ubx_block_unregister(ni, "cppdemo/cppdemo");
}

UBX_MODULE_INIT(cppdemo_init)
UBX_MODULE_CLEANUP(cppdemo_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
