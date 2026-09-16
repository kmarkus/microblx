/*
 * vstore: a single-slot, non-atomic value store iblock
 */

#undef UBX_DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ubx.h"

char vstore_meta[] =
	"{ doc='single-slot value store for same-thread connections',"
	"  description=[[ A connection whose writer and reader are stepped by the"
	"                 same trigger needs no queue and no synchronisation: the"
	"                 write always completes before the read begins, and there"
	"                 is never a second writer or a second reader. vstore is"
	"                 that case reduced to what it actually needs -- one buffer,"
	"                 one flag, a memcpy each way."
	""
	"                 Against lfrb this removes four atomic read-modify-writes"
	"                 per connection per cycle (two per direction), which on an"
	"                 ARMv8.0 core without LSE is four out-of-line helper calls"
	"                 each taking an LL/SC retry loop, and it collapses the"
	"                 freeq/usedq/slot/elem indirection into one allocation, so"
	"                 the buffer shares a cache line with the flag that guards"
	"                 it."
	""
	"                 USE ONLY where writer and reader run in the same thread."
	"                 There is no ordering here at all, so a cross-thread pair"
	"                 will read torn data. If in doubt use lfrb: it is correct"
	"                 everywhere. The overwrites port reports writes that"
	"                 landed before the previous value was read, which in"
	"                 correct use never happens -- a nonzero count means the"
	"                 precondition does not hold for that connection. ]],"
	"  version=0.1,"
	"  hard_real_time=true,"
	"}";

ubx_proto_config_t vstore_config[] = {
	{ .name="type_name", .type_name="char",     .min=1,         .doc="name of registered microblx type to transport" },
	{ .name="data_len",  .type_name="uint32_t", .min=0, .max=1, .doc="array length (multiplier) of data (default: 1)" },
	{ 0 },
};

ubx_proto_port_t vstore_ports[] = {
	{ .name = "overwrites", .out_type_name = "unsigned long", .doc = "writes that overwrote an unread value. Nonzero means writer and reader are not the same-thread pair vstore assumes. Output only upon change." },
	{ 0 },
};

struct vstore_info {
	const ubx_type_t *type;
	long data_len;			/* array length */
	unsigned long overwrites;
	ubx_port_t *p_overwrites;
	int fresh;			/* an unread value is held */
	long len;			/* array elements actually written */
	uint8_t data[];			/* tail-allocated, see vstore_init */
};

int vstore_init(ubx_block_t *i)
{
	int ret = -1;
	long len;
	const uint32_t *val;
	const char *type_name;
	const ubx_type_t *type;
	long data_len;
	struct vstore_info *inf;

	len = cfg_getptr_uint32(i, "data_len", &val);
	if (len < 0)
		return len;

	data_len = (len > 0) ? *val : 1;

	if (data_len <= 0) {
		ubx_err(i, "EINVALID_CONFIG: data_len=%ld", data_len);
		return EINVALID_CONFIG;
	}

	len = cfg_getptr_char(i, "type_name", &type_name);
	assert(len > 0);

	type = ubx_type_get(i->nd, type_name);

	if (type == NULL) {
		ubx_err(i, "EINVALID_CONFIG: unknown type %s", type_name);
		return EINVALID_CONFIG;
	}

	/* One allocation for the bookkeeping and the payload together: the
	 * whole point of this block is to touch as few cache lines as lfrb
	 * touches many, so the flag and the data it guards must not be two
	 * separate mallocs. */
	i->private_data = calloc(1, sizeof(struct vstore_info) + data_len * type->size);

	if (i->private_data == NULL) {
		ubx_err(i, "EOUTOFMEM: failed to alloc vstore_info");
		return EOUTOFMEM;
	}

	inf = (struct vstore_info *)i->private_data;
	inf->type = type;
	inf->data_len = data_len;

	inf->p_overwrites = ubx_port_get(i, "overwrites");
	assert(inf->p_overwrites);

	ubx_debug(i, "%s: %s [%ld], %zu bytes total", __func__, type_name,
		  data_len, sizeof(struct vstore_info) + data_len * type->size);

	ret = 0;
	return ret;
}

void vstore_cleanup(ubx_block_t *i)
{
	free(i->private_data);
	i->private_data = NULL;
}

void vstore_write(ubx_block_t *i, const ubx_data_t *msg)
{
	struct vstore_info *inf = (struct vstore_info *)i->private_data;
	long writelen;

	if (inf->type != msg->type) {
		ubx_err(i, "%s: invalid message type %s", __func__, msg->type->name);
		return;
	}

	if (inf->fresh) {
		inf->overwrites++;
		write_ulong(inf->p_overwrites, &inf->overwrites);
	}

	if (msg->len > inf->data_len) {
		ubx_err(i, "%s: only copying %ld of %lu array elements",
			__func__, inf->data_len, msg->len);
	}

	writelen = MIN(msg->len, inf->data_len);

	memcpy(inf->data, msg->data, inf->type->size * writelen);

	/* Record what was actually written, not what the buffer can hold: a
	 * writer may send fewer array elements than data_len, and the reader
	 * must not be handed the stale tail as if it were data. lfrb carries
	 * the same thing per element as elem->data_len. */
	inf->len = writelen;

	/* Plain store, deliberately. Writer and reader are the same thread, so
	 * program order is all the ordering there is to have. */
	inf->fresh = 1;
}

long vstore_read(ubx_block_t *i, ubx_data_t *msg)
{
	struct vstore_info *inf = (struct vstore_info *)i->private_data;
	long readlen;

	if (inf->type != msg->type) {
		ubx_err(i, "%s: invalid message type %s", __func__, msg->type->name);
		return EINVALID_TYPE;
	}

	/* No unread value: same contract as lfrb's empty usedq. Blocks commonly
	 * gate on a positive return to distinguish "a new value arrived" from
	 * "nothing this step" -- a control block re-applying a stale setpoint
	 * every step is a real failure mode -- so a store that always returned
	 * its held value would silently break them. */
	if (!inf->fresh)
		return 0;

	if ((long)msg->len < inf->len) {
		ubx_err(i, "%s: only copying %lu array elements of %ld",
			__func__, msg->len, inf->len);
	}

	readlen = MIN((long)msg->len, inf->len);

	memcpy(msg->data, inf->data, inf->type->size * readlen);

	inf->fresh = 0;

	return readlen;
}

ubx_proto_block_t vstore_comp = {
	.name = "ubx/vstore",
	.type = BLOCK_TYPE_INTERACTION,
	.meta_data = vstore_meta,
	.configs = vstore_config,
	.ports = vstore_ports,

	.init = vstore_init,
	.cleanup = vstore_cleanup,

	/* iops */
	.write = vstore_write,
	.read = vstore_read,
};

int vstore_mod_init(ubx_node_t *nd)
{
	return ubx_block_register(nd, &vstore_comp);
}

void vstore_mod_cleanup(ubx_node_t *nd)
{
	ubx_block_unregister(nd, "ubx/vstore");
}

UBX_MODULE_INIT(vstore_mod_init)
UBX_MODULE_CLEANUP(vstore_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
