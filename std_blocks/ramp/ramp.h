/*
 * ramp microblx function block
 */

#ifndef RAMP_T
 #error "RAMP_T undefined"
#endif

#ifndef BLOCK_NAME
 #define BLOCK_NAME RAMP_T
#endif

#include <ubx.h>

UBX_MODULE_LICENSE_SPDX(MIT)

/* includes types and type metadata */

ubx_type_t types[] = {
	{ NULL },
};

/* block meta information */
char ramp_meta[] =
	" { doc='',"
	"   real-time=true,"
	"}";

/* declaration of block configuration */
ubx_config_t ramp_config[] = {
	{ .name="start", .type_name = QUOTE(RAMP_T), .doc="ramp starting value (def 0)" },
	{ .name="slope", .type_name = QUOTE(RAMP_T), .doc="rate of change (def: 1)" },
	{ NULL },
};

/* declaration port block ports */
ubx_port_t ramp_ports[] = {
	{ .name="out", .out_type_name=QUOTE(RAMP_T), .out_data_len=1, .doc="ramp generator output"  },
	{ NULL },
};

/* declare a struct port_cache */
struct ramp_port_cache {
	ubx_port_t* out;
};

/* declare a helper function to update the port cache this is necessary
 * because the port ptrs can change if ports are dynamically added or
 * removed. This function should hence be called after all
 * initialization is done, i.e. typically in 'start'
 */
static void update_port_cache(ubx_block_t *b, struct ramp_port_cache *pc)
{
	pc->out = ubx_port_get(b, "out");
}

/* for each port type, declare convenience functions to read/write from ports */
def_write_fun(write_out, RAMP_T)

/* block operation forward declarations */
int ramp_init(ubx_block_t *b);
int ramp_start(ubx_block_t *b);
void ramp_stop(ubx_block_t *b);
void ramp_cleanup(ubx_block_t *b);
void ramp_step(ubx_block_t *b);


/* put everything together */
ubx_block_t ramp_block = {
	.name = "ramp_" QUOTE(BLOCK_NAME),
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = ramp_meta,
	.configs = ramp_config,
	.ports = ramp_ports,

	/* ops */
	.init = ramp_init,
	.start = ramp_start,
	.stop = ramp_stop,
	.cleanup = ramp_cleanup,
	.step = ramp_step,
};


/* ramp module init and cleanup functions */
int ramp_mod_init(ubx_node_info_t* ni)
{
	DBG(" ");
	int ret = -1;
	ubx_type_t *tptr;

	for(tptr=types; tptr->name!=NULL; tptr++) {
		if(ubx_type_register(ni, tptr) != 0) {
			goto out;
		}
	}

	if(ubx_block_register(ni, &ramp_block) != 0)
		goto out;

	ret=0;
out:
	return ret;
}

void ramp_mod_cleanup(ubx_node_info_t *ni)
{
	DBG(" ");
	const ubx_type_t *tptr;

	for(tptr=types; tptr->name!=NULL; tptr++)
		ubx_type_unregister(ni, tptr->name);

	ubx_block_unregister(ni, "ramp_" QUOTE(BLOCK_NAME));
}

/* declare module init and cleanup functions, so that the ubx core can
 * find these when the module is loaded/unloaded */
UBX_MODULE_INIT(ramp_mod_init)
UBX_MODULE_CLEANUP(ramp_mod_cleanup)
