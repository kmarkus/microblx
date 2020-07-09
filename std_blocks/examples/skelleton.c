/* An empty skelleton block */

#define UBX_DEBUG

/* Note: for builds out of microblx set this to <ubx/ubx.h> */
#include "ubx.h"

char skel_meta[] = "{ doc='an almost empty skelleton block' }";

ubx_proto_config_t skel_config[] = {
	{ .name = "foo", .type_name = "struct foo_type", .min=0, .max=1, .doc="foo to output" },
	{ .name = "loglevel", .type_name = "int" },
	{ 0 },
};

ubx_proto_port_t skel_ports[] = {
	{ .name = "in",  .in_type_name = "struct foo_type", .doc="just an in foo" },
	{ .name = "out",  .out_type_name = "struct foo_type", .doc="just an out foo" },
	{ 0 },
};

/* add custom stuct foo_type type */
#include "types/foo_type.h"
#include "types/foo_type.h.hexarr"

ubx_type_t skel_types[] =  {
	def_struct_type(struct foo_type, &foo_type_h),
};

/* the following macro defines:
 *
 * long cfg_getptr_foo_type(ubx_block_t *b, const char* cfgname, struct foo_type** valptr)
 *
 * void write_foo_type(ubx_port_t *p, const struct foo_type *val)
 * long read_foo_type(ubx_port_t *p, struct foo_type *val)
 *
 * void write_foo_type_array(ubx_port_t *p, const struct foo_type *val, const int len)
 * long read_foo_type_array(ubx_port_t *p, struct foo_type* val, )
 */
def_type_accessors(foo_type, struct foo_type)

/* block instance state */
struct skel_info {
	const struct foo_type *foo;
	ubx_port_t *p_in, *p_out;	/* cached pointers to port */
};

int skel_init(ubx_block_t *b)
{
	long len;

	b->private_data = calloc(1, sizeof(struct skel_info));

	if (b->private_data == NULL) {
		ubx_crit(b, "ENOMEM");
		return EOUTOFMEM;
	}

	struct skel_info *inf = (struct skel_info *)b->private_data;

	len = cfg_getptr_foo_type(b, "foo", &inf->foo);
	assert(len > 0);

	inf->p_out = ubx_port_get(b, "foo_out");
	assert(inf->p_out != NULL);

	inf->p_in = ubx_port_get(b, "foo_in");
	assert(inf->p_in != NULL);

	return 0;
}

void skel_cleanup(ubx_block_t *b)
{
	free(b->private_data);
}

int skel_start(ubx_block_t *b)
{
	ubx_debug(b, "%s", __func__);
	return 0;
}

void skel_stop(ubx_block_t *b)
{
	ubx_debug(b, "%s", __func__);
}

void skel_step(ubx_block_t *b)
{
	long len;
	struct foo_type f;
	struct skel_info *inf = (struct skel_info *)b->private_data;

	ubx_debug(b, "%s", __func__);

	len = read_foo_type(inf->p_in, &f);

	if (len == 0) {
		write_foo_type(inf->p_in, inf->foo);
	} else if (len > 0) {
		write_foo_type(inf->p_out, &f);
	} else {
		ubx_err(b, "error reading port foo_in");
	}
}

ubx_proto_block_t skel_comp = {
	.name = "ubx/skelleton",
	.meta_data = skel_meta,
	.type = BLOCK_TYPE_COMPUTATION,

	.ports = skel_ports,
	.configs = skel_config,

	.init = skel_init,
	.cleanup = skel_cleanup,
	.start = skel_start,
	.stop = skel_stop,
	.step = skel_step,
};

int skel_module_init(ubx_node_t *nd)
{
	for (long unsigned int i=0; i<ARRAY_SIZE(skel_types); i++) {
		if (ubx_type_register(nd, &skel_types[i]))
			return -1;
	}

	return ubx_block_register(nd, &skel_comp);
}

void skel_module_cleanup(ubx_node_t *nd)
{
	for (long unsigned int i=0; i<ARRAY_SIZE(skel_types);	i++)
		ubx_type_unregister(nd, skel_types[i].name);

	ubx_block_unregister(nd, "ubx/threshold");
}

UBX_MODULE_INIT(skel_module_init)
UBX_MODULE_CLEANUP(skel_module_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
