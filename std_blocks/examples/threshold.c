/* A minimal, well (over-) documented example function block that
 * checks a threshold and outputs events */

#define UBX_DEBUG

/* Note: for builds out of microblx set this to <ubx/ubx.h> */
#include "ubx.h"

/* threshold block meta data - used by modinfo tool */
char thres_meta[] =
	"{ doc='A minimal block that checks whether its input is above a threshold' }";

/* Configurations */
ubx_proto_config_t thres_config[] = {
	{ .name = "threshold", .type_name = "double", .min = 1, .max = 1, .doc="threshold to check" },

	/* if a 'loglevel' config is defined, it will automatically
	 * affect the block loglevel. If unset or undefined the global
	 * loglevel is used */
	{ .name = "loglevel", .type_name = "int" },
	{ 0 },
};

/* Ports */
ubx_proto_port_t thres_ports[] = {
	{ .name = "in",  .in_type_name = "double", .doc="input signal to compare" },
	{ .name = "state", .out_type_name = "int", .doc="1 if above threshold, 0 if below" },
	{ .name = "event", .out_type_name = "struct thres_event", .doc="threshold crossing events" },
	{ 0 },
};

/* declare struct thres_event as a custom type. In addition, it must
 * be registered/unregistered in the module init/cleanup at the end of
 * this file.
 */
#include "types/thres_event.h"
#include "types/thres_event.h.hexarr"

ubx_type_t thres_event_type = def_struct_type(struct thres_event, &thres_event_h);

/* define port read/write helpers for the the thres_event type. This
 * will define the functions
 *    void write_thres_event(ubx_port_t*, const struct thres_event*)
 *    long read_thres_event(ubx_port_t*, struct thres_event*)
 */
def_port_accessors(thres_event, struct thres_event)

/* block instance state. To be allocated in the init hook */
struct thres_info {
	const double *threshold;		/* pointer to configuration */
	int state;				/* current state of threshold: 1 if above, 0 if below */
	ubx_port_t *pin, *pstate, *pevent;	/* cached pointers to port */
};

/* init hook: allocate, check configs, cache ports */
int thres_init(ubx_block_t *b)
{
	long len;

	b->private_data = calloc(1, sizeof(struct thres_info));

	if (b->private_data == NULL) {
		ubx_crit(b, "ENOMEM");
		return EOUTOFMEM;
	}

	struct thres_info *inf = (struct thres_info *)b->private_data;

	/* retrieve ptr to threshold config and check that it was
	 * found. The min/max specifiers on the config above ensure
	 * that a configuration value will be available here. The
	 * assert only guards against mistyping the configs name */
	len = cfg_getptr_double(b, "threshold", &inf->threshold);
	assert(len > 0);

	/* cache the port ptrs: avoids repeated lookups in step */
	inf->pin = ubx_port_get(b, "in");
	assert(inf->pin);

	inf->pstate = ubx_port_get(b, "state");
	assert(inf->pstate);

	inf->pevent = ubx_port_get(b, "event");
	assert(inf->pevent);

	return 0;
}

/* cleanup hook: free allocations, close devices, ... */
void thres_cleanup(ubx_block_t *b)
{
	free(b->private_data);
}

/* step hook: the main functionality of the block is here */
void thres_step(ubx_block_t *b)
{
	long len;
	int state;
	double inval;
	struct thres_event ev;

	struct thres_info *inf = (struct thres_info *)b->private_data;

	len = read_double(inf->pin, &inval);

	if (len < 0) {
		ubx_err(b, "error reading in port");
		return;
	} else if (len == 0) {
		return;	/* no data on port */
	}

	state = (inval > *inf->threshold) ? 1 : 0;

	if (state != inf->state) {
		ubx_debug(b, "threshold %f passed in %s direction",
			  *inf->threshold,
			  (state == 1) ? "rising" : "falling");
		ev.dir = state;
		ubx_gettime(&ev.ts);
		write_thres_event(inf->pevent, &ev);
	}

	write_int(inf->pstate, &state);
	inf->state = state;
}


/* put everything together */
ubx_proto_block_t thres_comp = {
	.name = "ubx/threshold",
	.meta_data = thres_meta,
	.type = BLOCK_TYPE_COMPUTATION,

	.ports = thres_ports,
	.configs = thres_config,

	.init = thres_init,
	.cleanup = thres_cleanup,
	.step = thres_step,
};

/* module init: register blocks and types */
int thres_module_init(ubx_node_t *nd)
{
	if (ubx_type_register(nd, &thres_event_type) ||
	    ubx_block_register(nd, &thres_comp)) {
		return -1;
	}
	return 0;
}

/* module cleanup: unregister blocks and types */
void thres_module_cleanup(ubx_node_t *nd)
{
	ubx_type_unregister(nd, thres_event_type.name);
	ubx_block_unregister(nd, "ubx/threshold");
}

/* declare the module init and cleanup function */
UBX_MODULE_INIT(thres_module_init)
UBX_MODULE_CLEANUP(thres_module_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
