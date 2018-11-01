/*
 * A demo C++ block
 */

#define DEBUG 1

#include <iostream>
#include <ubx.h>

#include "types/cpp_demo_type.h"
#include "types/cpp_demo_type.h.hexarr"

using namespace std;

/* function block meta-data
 * used by higher level functions.
 */
char cppdemo_meta[] =
	"{ doc='A cppdemo number generator function block',"
	"  realtime=true,"
	"}";

/* configuration
 * upon cloning the following happens:
 *   - value.type is resolved
 *   - value.data will point to a buffer of size value.len*value.type->size
 *
 * if an array is required, then .value = { .len=<LENGTH> } can be used.
 */
ubx_config_t cppdemo_config[] = {
	ubx_config_cpp("test_conf", "double", "a test config value"),
	{ NULL },
};


ubx_port_t cppdemo_ports[] = {
	// name, in_type, in_data_len, out_type, out_data_len
	ubx_port_cpp("foo", "unsigned int", 1, NULL, 0, "Out port writing foo unsigned ints"),
	ubx_port_cpp("bar", NULL, 0, "unsigned int", 1, "In port reading bar unsigned ints"),
	{ NULL },
};

/* define a type, registration in module hooks below */
struct ubx_type_cpp cpp_demo_type("struct cpp_demo_type",
				  TYPE_CLASS_STRUCT,
				  sizeof(struct cpp_demo_type),
				  (void*) &cpp_demo_type_h);

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

static void cppdemo_stop(ubx_block_t *c)
{
	cout << "cppdemo_stop: hi from " << c->name << endl;
}

static void cppdemo_step(ubx_block_t *c) {
	cout << "cppdemo_step: hi from " << c->name << endl;
}


/* put everything together */

ubx_block_t cppdemo_comp = ubx_block_cpp(
	"cppdemo/cppdemo",
	BLOCK_TYPE_COMPUTATION,
	cppdemo_meta,
	cppdemo_config,
	cppdemo_ports,
	cppdemo_init,
	cppdemo_start,
	cppdemo_stop,
	cppdemo_step,
	cppdemo_cleanup);

static int cppdemo_init(ubx_node_info_t* ni)
{
	DBG(" ");
	ubx_type_register(ni, &cpp_demo_type);
	return ubx_block_register(ni, &cppdemo_comp);
}

static void cppdemo_cleanup(ubx_node_info_t *ni)
{
	DBG(" ");
	ubx_type_unregister(ni, "cpp_demo_type");
	ubx_block_unregister(ni, "cppdemo/cppdemo");
}

UBX_MODULE_INIT(cppdemo_init)
UBX_MODULE_CLEANUP(cppdemo_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
