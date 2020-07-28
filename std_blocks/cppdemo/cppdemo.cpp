/*
 * A demo C++ block
 */

#undef UBX_DEBUG

#include <iostream>
#include <ubx.h>

#include "types/cpp_demo_type.h"
#include "types/cpp_demo_type.h.hexarr"

using namespace std;

char cppdemo_meta[] =
    "{ doc='A cppdemo number generator function block',"
    "  realtime=true,"
    "}";

ubx_proto_config_t cppdemo_config[] = {
    { .name="test_conf", 
			.type_name="double",  
			.attrs=0,
	 		.min=0,
	 		.max=0, 
			.doc="a test config value"
 },
    { 0 },
};

ubx_proto_port_t cppdemo_ports[] = {// filled in also unused fields to make compatible with g++
    { name:"foo",attrs:0, out_type_name:"", out_data_len:0, in_type_name:"unsigned int",in_data_len:0, doc:"Out port writing foo unsigned ints" },
    { name:"bar",attrs:0, out_type_name:"unsigned int", out_data_len:0,in_type_name:"",in_data_len:0,  doc:"In port reading bar unsigned ints" },
    { 0 },
};

/* define a type, registration in module hooks below */
ubx_type_t cpp_demo_type =
    def_struct_type(struct cpp_demo_type, &cpp_demo_type_h);

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
    return 0;
}

static void cppdemo_stop(ubx_block_t *c)
{
    cout << "cppdemo_stop: hi from " << c->name << endl;
}

static void cppdemo_step(ubx_block_t *c) {
    cout << "cppdemo_step: hi from " << c->name << endl;
}


ubx_proto_block_t cppdemo_comp =
{
    .name = "cppdemo",
    .meta_data = cppdemo_meta,
		.attrs = 0,
    .type = BLOCK_TYPE_COMPUTATION,
    .configs = cppdemo_config,
		.ports = NULL, // fill in to make compatible with g++
    .init = cppdemo_init,
    .start = cppdemo_start,
    .stop = cppdemo_stop,
    .cleanup = cppdemo_cleanup,{
    .step = cppdemo_step,}
};

static int cppdemo_init(ubx_node_t* nd)
{
    ubx_type_register(nd, &cpp_demo_type);
    return ubx_block_register(nd, &cppdemo_comp);
}

static void cppdemo_cleanup(ubx_node_t *nd)
{
    ubx_type_unregister(nd, "cpp_demo_type");
    ubx_block_unregister(nd, "cppdemo/cppdemo");
}

UBX_MODULE_INIT(cppdemo_init)
UBX_MODULE_CLEANUP(cppdemo_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
