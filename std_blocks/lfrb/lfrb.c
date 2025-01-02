/*
 * lfq: a lock-free ring buffer iblock
 */

#undef UBX_DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <errno.h>
#include "ubx.h"
#include "lfq.h"

char lfrb_meta[] =
	"{ doc='lock-free ring buffer, buffered in process communication"
	"  description=[["
	"		 This version is stongly typed and should be preferred"
	"                This microblx iblock based on the minimal liblfq library"
	"  version=0.1,"
	"  hard_real_time=true,"
	"}";

ubx_proto_config_t lfrb_config[] = {
	{ .name="type_name",         .type_name="char",     .min=1,         .doc="name of registered microblx type to transport" },
	{ .name="data_len",          .type_name="uint32_t",         .max=1, .doc="array length (multiplier) of data (default: 1)" },
	{ .name="buffer_len",        .type_name="uint32_t", .min=0, .max=1, .doc="max number of data elements the buffer shall hold" },
	{ .name="allow_partial",     .type_name="int",      .min=0, .max=1, .doc="allow msgs with len<data_len. def: 0 (no)" },
	{ .name="loglevel_overruns", .type_name="int",      .min=0, .max=1, .doc="loglevel for reporting overflows (default: NOTICE, -1 to disable)" },
	{ 0 },
};

ubx_proto_port_t lfrb_ports[] = {
	{ .name = "overruns", .out_type_name = "unsigned long", .doc = "Number of buffer overruns. Value is output only upon change." },
	{ 0 },
};

struct lfrb_block_info {
	const ubx_type_t *type;		/* type of contained elements */
	long data_len;			/* buffer size of each element */
	long buffer_len;		/* number of elements */

	int allow_partial;
	unsigned long overruns;		/* stats */
	ubx_port_t *p_overruns;
	int loglevel_overruns;

	lfq_t freeq;
	lfq_t usedq;
};

struct lfrb_elem {
	long data_len;
	uint8_t data[0];
};

/* init */
int lfrb_init(ubx_block_t *i)
{
	int ret = -1;
	long len;
	const int *ival;
	const uint32_t *val;
	const char *type_name;
	struct lfrb_block_info *inf;
	struct lfrb_elem *elem;

	i->private_data = calloc(1, sizeof(struct lfrb_block_info));

	if (i->private_data == NULL) {
		ubx_err(i, "failed to alloc lfrb_block_info");
		ret = EOUTOFMEM;
		goto out;
	}

	inf = (struct lfrb_block_info *)i->private_data;

	/* read loglevel_overruns */
	len = cfg_getptr_int(i, "loglevel_overruns", &ival);
	assert(len>=0);

	inf->loglevel_overruns = (len==0) ? UBX_LOGLEVEL_NOTICE : *ival;

	if (inf->loglevel_overruns < -1 || inf->loglevel_overruns > UBX_LOGLEVEL_DEBUG) {
		ubx_err(i, "EINVALID_CONFIG: loglevel_overruns:	%i",
			inf->loglevel_overruns);
		ret = EINVALID_CONFIG;
		goto out_free_priv_data;
	}

	/* read and check buffer_len config */
	len = cfg_getptr_uint32(i, "buffer_len", &val);
	assert(len>=0);

	inf->buffer_len = (len > 0) ? *val : 1;

	if (inf->buffer_len == 0) {
		ubx_err(i, "EINVALID_CONFIG: buffer_len=0");
		ret = EINVALID_CONFIG;
		goto out_free_priv_data;
	}

	/* read and check data_len config */
	len = cfg_getptr_uint32(i, "data_len", &val);
	if (len < 0)
		goto out_free_priv_data;

	inf->data_len = (len > 0) ? *val : 1;

	len = cfg_getptr_char(i, "type_name", &type_name);

	inf->type = ubx_type_get(i->nd, type_name);

	if (inf->type == NULL) {
		ubx_err(i, "EINVALID_CONFIG: unknown type %s", type_name);
		ret = EINVALID_CONFIG;
		goto out_free_priv_data;
	}

	/* initialize queues */
	ubx_debug(i, "%s: alloc ringbuf of %lu x %s [%lu]",
		  __func__, inf->buffer_len, type_name, inf->data_len);

	/* free_queue */
	ret = lfq_init(&inf->freeq, inf->buffer_len);

	if (ret != 0) {
		ubx_debug(i, "%s: lfq_init failed for freeq: %s",
			  __func__, strerror(-ret));
		ret = EOUTOFMEM;
		goto out_free_priv_data;
	}

	/* used_queue */
	ret = lfq_init(&inf->usedq, inf->buffer_len);

	if (ret != 0) {
		ubx_debug(i, "%s: lfq_init failed for usedq: %s",
			  __func__, strerror(-ret));
		ret = EOUTOFMEM;
		goto out_free_freeq;
	}

	/* alloc elements and add them to freeq */
	for (int n=0; n<inf->buffer_len; n++) {
		elem = calloc(1, inf->data_len * inf->type->size + sizeof(struct lfrb_elem));

		if (elem==NULL) {
			ubx_debug(i, "%s: failed to alloc elem: %s",
				  __func__, strerror(-ret));
			ret = EOUTOFMEM;
			goto out_free_elem;
		}

		ret = lfq_enqueue(&inf->freeq, elem);

		if (ret != 0) {
			ubx_debug(i, "%s: failed to enqueue elem #%d in freeq: %s", __func__, n, strerror(-ret));
			goto out_free_elem;
		}
	}

	/* read allow_partial */
	len = cfg_getptr_int(i, "allow_partial", &ival);
	assert(len>=0);
	inf->allow_partial = (len>0) ? *ival : 0;

	/* cache port ptrs */
	inf->p_overruns = ubx_port_get(i, "overruns");
	assert(inf->p_overruns);

	ret = 0;
	goto out;

out_free_elem:
	while(lfq_dequeue(&inf->freeq, (void**) &elem) != -ENODATA)
		free(elem);

	lfq_free(&inf->usedq);
out_free_freeq:
	lfq_free(&inf->freeq);

out_free_priv_data:
	free(i->private_data);
out:
	return ret;
}

/* cleanup */
void lfrb_cleanup(ubx_block_t *i)
{
	struct lfrb_block_info *inf;
	struct lfrb_elem *elem;

	inf = (struct lfrb_block_info *)i->private_data;

	while(lfq_dequeue(&inf->usedq, (void**) &elem) != -ENODATA)
		free(elem);

	while(lfq_dequeue(&inf->freeq, (void**) &elem) != -ENODATA)
		free(elem);

	lfq_free(&inf->usedq);
	lfq_free(&inf->freeq);

	free(inf);
}

/* write */
void lfrb_write(ubx_block_t *i, const ubx_data_t *msg)
{
	int ret;
	long len;
	struct lfrb_block_info *inf;
	struct lfrb_elem *elem;

	inf = (struct lfrb_block_info *)i->private_data;

	if (inf->type != msg->type) {
		ubx_err(i, "invalid message type %s", msg->type->name);
		goto out;
	}

	if (inf->allow_partial) {
		if (msg->len > inf->data_len) {
			ubx_err(i, "msg array len too large: is: %lu, capacity: %lu",
				msg->len, inf->data_len);
			goto out;
		}
	} else {
		if (msg->len != inf->data_len) {
			ubx_err(i, "EINVALID_DATA_LEN: msg len %lu != data_len %lu",
				msg->len, inf->data_len);
			goto out;
		}
	}

	while (1) {
		ret = lfq_dequeue(&inf->freeq, (void**) &elem);

		if (ret == -ENODATA) {
			ret = lfq_dequeue(&inf->usedq, (void**) &elem);

			if (ret == -ENODATA)
				continue;

			inf->overruns++;

			write_ulong(inf->p_overruns, &inf->overruns);

			if (inf->loglevel_overruns >= 0)
				ubx_block_log(inf->loglevel_overruns, i, "buffer overrun: #%ld", inf->overruns);

		}

		break; /* we have an element */
	}

	len = data_size(msg);
	memcpy(elem->data, msg->data, len);
	elem->data_len = msg->len;

	ubx_debug(i, "%s: %s: copied %ld bytes into elem %p", __func__, i->name, len, elem);

	lfq_enqueue(&inf->usedq, elem);

 out:
	return;
}

/* where to check whether the msg->data len is long enough? */
long lfrb_read(ubx_block_t *i, ubx_data_t *msg)
{
	int ret;
	unsigned long readlen, readsz;
	struct lfrb_block_info *inf;
	struct lfrb_elem *elem;

	inf = (struct lfrb_block_info *)i->private_data;

	if (inf->type != msg->type) {
		ubx_err(i, "%s: invalid message type %s", __func__, msg->type->name);
		return EINVALID_TYPE;
	}

	ret = lfq_dequeue(&inf->usedq, (void**) &elem);

	if (ret == -ENODATA) {
		return 0;
	} else if (ret != 0) {
		ubx_err(i, "%s: failed: %s", __func__, strerror(-ret));
		return ret;
	}
	
	if (msg->len < elem->data_len) {
		ubx_err(i, "%s: only copying %lu array elements of %lu",
			__func__, msg->len, elem->data_len);
	}

	readlen = MIN(msg->len, elem->data_len);
	readsz = inf->type->size * readlen;

	ubx_debug(i, "%s: %s: copying %ld bytes from elem %p", __func__, i->name, readsz, elem);

	memcpy(msg->data, elem->data, readsz);

	ret = lfq_enqueue(&inf->freeq, elem);
	if (ret != 0)
		ubx_err(i, "%s: failed to enqueue read elem to freeq: %s",
			__func__, strerror(-ret));

	return readlen;
}

/* put everything together */
ubx_proto_block_t lfrb_comp = {
	.name = "ubx/lfrb",
	.type = BLOCK_TYPE_INTERACTION,
	.meta_data = lfrb_meta,
	.configs = lfrb_config,
	.ports = lfrb_ports,

	.init = lfrb_init,
	.cleanup = lfrb_cleanup,

	/* iops */
	.write = lfrb_write,
	.read = lfrb_read,
};

int lfrb_mod_init(ubx_node_t *nd)
{
	return ubx_block_register(nd, &lfrb_comp);
}

void lfrb_mod_cleanup(ubx_node_t *nd)
{
	ubx_block_unregister(nd, "ubx/lfrb");
}

UBX_MODULE_INIT(lfrb_mod_init)
UBX_MODULE_CLEANUP(lfrb_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
