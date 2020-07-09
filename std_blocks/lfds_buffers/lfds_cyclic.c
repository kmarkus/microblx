/*
 * A lock-free interaction
 */

#undef UBX_DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <liblfds611.h>

#include "ubx.h"

/* meta-data */
char cyclic_meta[] =
	"{ doc='High performance scalable, lock-free cyclic, buffered in process communication"
	"  description=[["
	"		 This version is stongly typed and should be preferred"
	"                This microblx iblock is based on based on liblfds"
	"                ringbuffer (v0.6.1.1) (www.liblfds.org)]],"
	"  version=0.01,"
	"  hard_real_time=true,"
	"}";

/* configuration */
ubx_proto_config_t cyclic_config[] = {
	{ .name = "type_name", .type_name = "char", .min = 1, .doc = "name of registered microblx type to transport" },
	{ .name = "data_len", .type_name = "uint32_t", .max = 1, .doc = "array length (multiplier) of data (default: 1)" },
	{ .name = "buffer_len", .type_name = "uint32_t", .min = 1, .max = 1, .doc = "max number of data elements the buffer shall hold" },
	{ .name = "allow_partial", .type_name = "int", .min = 0, .max = 1, .doc = "allow msgs with len<data_len. def: 0 (no)" },
	{ .name = "loglevel_overruns", .type_name = "int", .min = 0, .max = 1, .doc = "loglevel for reporting overflows (default: NOTICE, -1 to disable)" },
	{ 0 },
};

ubx_proto_port_t cyclic_ports[] = {
	/* generic arm+base */
	{ .name = "overruns", .out_type_name = "unsigned long", .doc = "Number of buffer overruns. Value is output only upon change." },
	{ 0 },
};

/* interaction private data */
struct cyclic_block_info {
	const ubx_type_t *type;		/* type of contained elements */
	long data_len;			/* buffer size of each element */
	long buffer_len;		/* number of elements */

	struct lfds611_ringbuffer_state *rbs;

	int allow_partial;
	unsigned long overruns;		/* stats */
	ubx_port_t *p_overruns;
	int loglevel_overruns;
};

struct cyclic_elem_header {
	long data_len;
	uint8_t data[0];
};

int cyclic_data_elem_init(void **user_data, void *user_state)
{
	struct cyclic_block_info *inf = (struct cyclic_block_info *)user_state;

	*user_data = calloc(1, inf->data_len * inf->type->size +
			    sizeof(struct cyclic_elem_header));

	return (*user_data == NULL) ? 0 : 1;
}

void cyclic_data_elem_del(void *user_data, void *user_state)
{
	(void) user_state;
	free(user_data);
}


/* init */
int cyclic_init(ubx_block_t *i)
{
	int ret = -1;
	long len;
	const int *ival;
	const uint32_t *val;
	const char *type_name;
	struct cyclic_block_info *inf;

	i->private_data = calloc(1, sizeof(struct cyclic_block_info));
	if (i->private_data == NULL) {
		ubx_err(i, "failed to alloc cyclic_block_info");
		ret = EOUTOFMEM;
		goto out;
	}

	inf = (struct cyclic_block_info *)i->private_data;

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

	if (*val == 0) {
		ubx_err(i, "EINVALID_CONFIG: buffer_len=0");
		ret = EINVALID_CONFIG;
		goto out_free_priv_data;
	}

	inf->buffer_len = *val;

	/* read and check data_len config */
	len = cfg_getptr_uint32(i, "data_len", &val);
	if (len < 0)
		goto out_free_priv_data;

	inf->data_len = (len > 0) ? *val : 1;

	len = cfg_getptr_char(i, "type_name", &type_name);

	inf->type = ubx_type_get(i->nd, type_name);

	if (inf->type == NULL) {
		ubx_err(i, "EINVALID_CONFIG: unkown type %s", type_name);
		ret = EINVALID_CONFIG;
		goto out_free_priv_data;
	}

	ubx_debug(i, "alloc ringbuf of %lu x %s [%lu]",
		  inf->buffer_len, type_name, inf->data_len);

	if (lfds611_ringbuffer_new(&inf->rbs, inf->buffer_len,
				   cyclic_data_elem_init, inf) == 0) {
		ubx_err(i, "EOUTOFMEM: ringbuf of %lu x %s [%lu]",
			inf->buffer_len, type_name, inf->data_len);
		ret = EOUTOFMEM;
		goto out_free_priv_data;
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

 out_free_priv_data:
	free(i->private_data);
 out:
	return ret;
}

/* cleanup */
void cyclic_cleanup(ubx_block_t *i)
{
	struct cyclic_block_info *inf;

	inf = (struct cyclic_block_info *)i->private_data;
	lfds611_ringbuffer_delete(inf->rbs, cyclic_data_elem_del, inf);
	free(inf);
}

/* write */
void cyclic_write(ubx_block_t *i, const ubx_data_t *msg)
{
	int ret;
	long len;
	struct cyclic_block_info *inf;
	struct lfds611_freelist_element *elem;
	struct cyclic_elem_header *hd;

	inf = (struct cyclic_block_info *)i->private_data;

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

	elem = lfds611_ringbuffer_get_write_element(inf->rbs, &elem, &ret);

	if (ret) {
		inf->overruns++;

		write_ulong(inf->p_overruns, &inf->overruns);

		if (inf->loglevel_overruns >= 0) {
			ubx_block_log(inf->loglevel_overruns, i,
				      "buffer overrun: #%ld", inf->overruns);
		}
	}

	/* write */
	hd = lfds611_freelist_get_user_data_from_element(elem, NULL);

	len = data_size(msg);
	memcpy(hd->data, msg->data, len);
	hd->data_len = msg->len;

	ubx_debug(i, "copying %ld bytes", len);

	/* release element */
	lfds611_ringbuffer_put_write_element(inf->rbs, elem);

 out:
	return;
}

/* where to check whether the msg->data len is long enough? */
long cyclic_read(ubx_block_t *i, ubx_data_t *msg)
{
	unsigned long readlen, readsz;
	struct cyclic_block_info *inf;
	struct lfds611_freelist_element *elem;
	struct cyclic_elem_header *hd;

	inf = (struct cyclic_block_info *)i->private_data;

	if (inf->type != msg->type) {
		ubx_err(i, "invalid message type %s", msg->type->name);
		return EINVALID_TYPE;
	}

	if (lfds611_ringbuffer_get_read_element(inf->rbs, &elem) == NULL) {
		return 0;
	}

	hd = lfds611_freelist_get_user_data_from_element(elem, NULL);

	if (msg->len < hd->data_len) {
		ubx_err(i, "only copying %lu array elements of %lu",
			msg->len, hd->data_len);
	}

	readlen = MIN(msg->len, hd->data_len);
	readsz = inf->type->size * readlen;

	ubx_debug(i, "%s: copying %ld bytes", i->name, readsz);

	memcpy(msg->data, hd->data, readsz);

	lfds611_ringbuffer_put_read_element(inf->rbs, elem);

	return readlen;
}

/* put everything together */
ubx_proto_block_t cyclic_comp = {
	.name = "ubx/lfds_cyclic",
	.type = BLOCK_TYPE_INTERACTION,
	.meta_data = cyclic_meta,
	.configs = cyclic_config,
	.ports = cyclic_ports,

	.init = cyclic_init,
	.cleanup = cyclic_cleanup,

	/* iops */
	.write = cyclic_write,
	.read = cyclic_read,
};

int cyclic_mod_init(ubx_node_t *nd)
{
	return ubx_block_register(nd, &cyclic_comp);
}

void cyclic_mod_cleanup(ubx_node_t *nd)
{
	ubx_block_unregister(nd, "ubx/lfds_cyclic");
}

UBX_MODULE_INIT(cyclic_mod_init)
UBX_MODULE_CLEANUP(cyclic_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
