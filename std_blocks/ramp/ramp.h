/*
 * ramp microblx function block
 */

#ifndef RAMP_T
 #error "RAMP_T undefined"
#endif

#ifndef BLOCK_NAME
 #define BLOCK_NAME RAMP_T
#endif

#include <string.h>
#include <math.h>
#include <ubx.h>

UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)

/* block meta information */
char ramp_meta[] =
	" { doc='Ramp generator block',"
	"   realtime=true,"
	"}";

/* declaration of block configuration */
ubx_proto_config_t ramp_config[] = {
	{ .name = "start", .type_name = QUOTE(RAMP_T), .doc = "ramp starting value (def 0)" },
	{ .name = "slope", .type_name = QUOTE(RAMP_T), .doc = "rate of change (def: 1)" },
	{ .name = "data_len", .type_name = "long", .doc = "length of output data (def: 1)" },
	{ 0 },
};

/* declaration port block ports */
ubx_proto_port_t ramp_ports[] = {
	{ .name = "out", .out_type_name = QUOTE(RAMP_T), .out_data_len = 1, .doc = "ramp generator output"  },
	{ 0 },
};

/* declare a struct port_cache */
struct ramp_port_cache {
	ubx_port_t *out;
};

/*
 * declare a helper function to update the port cache this is necessary
 * because the port ptrs can change if ports are dynamically added or
 * removed. This function should hence be called after all
 * initialization is done, i.e. typically in 'start'
 */
static void update_port_cache(ubx_block_t *b, struct ramp_port_cache *pc)
{
	pc->out = ubx_port_get(b, "out");
}

/* block operation forward declarations */
int ramp_init(ubx_block_t *b);
int ramp_start(ubx_block_t *b);
void ramp_cleanup(ubx_block_t *b);
void ramp_step(ubx_block_t *b);


ubx_proto_block_t ramp_block = {
	.name = "ramp_" QUOTE(BLOCK_NAME),
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = ramp_meta,
	.configs = ramp_config,
	.ports = ramp_ports,

	.init = ramp_init,
	.start = ramp_start,
	.cleanup = ramp_cleanup,
	.step = ramp_step,
};


int ramp_mod_init(ubx_node_t *nd)
{
	if (ubx_block_register(nd, &ramp_block) != 0)
		return -1;

	return 0;
}

void ramp_mod_cleanup(ubx_node_t *nd)
{
	ubx_block_unregister(nd, "ramp_" QUOTE(BLOCK_NAME));
}

/*
 * declare module init and cleanup functions, so that the ubx core can
 * find these when the module is loaded/unloaded
 */
UBX_MODULE_INIT(ramp_mod_init)
UBX_MODULE_CLEANUP(ramp_mod_cleanup)
