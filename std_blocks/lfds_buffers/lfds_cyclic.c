/*
 * A lock-free interaction
 */

/* #define DEBUG 1 */

#include <stdio.h>
#include <stdlib.h>
#include <liblfds611.h>

#include "ubx.h"

UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)

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
ubx_config_t cyclic_config[] = {
	{ .name="type_name", .type_name = "char", .doc="name of registered microblx type to transport" },
	{ .name="data_len", .type_name = "uint32_t", .doc="array length (multiplier) of data (default: 1)" },
	{ .name="buffer_len", .type_name = "uint32_t", .doc="max. number of data elements the buffer shall hold" },
	{ NULL },
};

ubx_port_t cyclic_ports[] = {
	/* generic arm+base */
	{ .name="overruns", .out_type_name="unsigned long", .doc="Number of buffer overruns. Value is output only upon change." },
	{ NULL },
};

/* interaction private data */
struct cyclic_block_info {
	const ubx_type_t* type;		/* type of contained elements */
	unsigned long data_len;		/* buffer size of each element */
	unsigned long buffer_len;	/* number of elements */

	struct lfds611_ringbuffer_state *rbs;

	unsigned long overruns;		/* stats */
	ubx_port_t* p_overruns;
};

struct cyclic_elem_header {
	unsigned long data_len;
	uint8_t data[0];
};

def_write_fun(write_ulong, unsigned long)

int cyclic_data_elem_init(void **user_data, void *user_state)
{
	struct cyclic_block_info* bbi = (struct cyclic_block_info*) user_state;
	*user_data=calloc(1, bbi->data_len*bbi->type->size+sizeof(struct cyclic_elem_header));
	return (*user_data==NULL) ? 0 : 1;
}

void cyclic_data_elem_del(void *user_data, void *user_state)
{
	(void) user_state;
	free(user_data);
}


/* init */
static int cyclic_init(ubx_block_t *i)
{
	int ret = -1;
	long int len;
	const uint32_t *val;
	const char *type_name;
	struct cyclic_block_info* bbi;

	if((i->private_data = calloc(1, sizeof(struct cyclic_block_info)))==NULL) {
		ubx_err(i, "failed to alloc cyclic_block_info");
		ret = EOUTOFMEM;
		goto out;
	}

	bbi = (struct cyclic_block_info*) i->private_data;

	/* read and check buffer_len config */
	len = cfg_getptr_uint32(i, "buffer_len", &val);

	if(len != 1) {
		ubx_err(i, "config 'buffer_len' %s",
			(len==0) ? "unconfigured" : "invalid");
		goto out_free_priv_data;
	}

	if(*val == 0) {
		ubx_err(i, "config buffer_len=0");
		ret = EINVALID_CONFIG;
		goto out_free_priv_data;
	}

	bbi->buffer_len = *val;

	/* read and check data_len config */
	if((len = cfg_getptr_uint32(i, "data_len", &val)) < 0)
		goto out_free_priv_data;

	bbi->data_len = (len>0) ? *val : 1;

	len = cfg_getptr_char(i, "type_name", &type_name);

	if (len <= 0 || type_name == NULL) {
		ubx_err(i, "config 'type_name' %s",
			(len==0) ? "unconfigured" : "invalid");
		goto out_free_priv_data;
	}

	bbi->type=ubx_type_get(i->ni, type_name);

	if(bbi->type==NULL) {
		ubx_err(i, "unkown type %s", type_name);
		ret = EINVALID_CONFIG;
		goto out_free_priv_data;
	}

	ubx_info(i, "alloc ringbuf of %lu x %s [%lu]",
		 bbi->buffer_len, type_name, bbi->data_len);

	if (lfds611_ringbuffer_new(&bbi->rbs,
				   bbi->buffer_len,
				   cyclic_data_elem_init, bbi)==0) {
			ubx_err(i, "alloc ringbuf of %lu x %s [%lu]",
			bbi->buffer_len, type_name, bbi->data_len);
		ret = EOUTOFMEM;
		goto out_free_priv_data;
	}

	/* cache port ptrs */
	bbi->p_overruns = ubx_port_get(i, "overruns");

	ret=0;
	goto out;

 out_free_priv_data:
	free(i->private_data);
 out:
	return ret;
};

/* cleanup */
static void cyclic_cleanup(ubx_block_t *i)
{
	struct cyclic_block_info *bbi;
	bbi = (struct cyclic_block_info*) i->private_data;
	lfds611_ringbuffer_delete(bbi->rbs, cyclic_data_elem_del, bbi);
	free(bbi);
}

/* write */
static void cyclic_write(ubx_block_t *i, ubx_data_t* msg)
{
	int ret;
	long len;
	struct cyclic_block_info *bbi;
	struct lfds611_freelist_element *elem;
	struct cyclic_elem_header *hd;

	bbi = (struct cyclic_block_info*) i->private_data;

	if(bbi->type != msg->type) {
		ubx_err(i, "invalid message type %s", msg->type->name);
		goto out;
	}

	if (msg->len > bbi->data_len) {
		ubx_err(i, "msg array len too large: is: %lu, capacity: %lu",
			msg->len, bbi->data_len);
		goto out;
	}

	elem = lfds611_ringbuffer_get_write_element(bbi->rbs, &elem, &ret);

	if(ret) {
		bbi->overruns++;
		write_ulong(bbi->p_overruns, &bbi->overruns);
		ubx_notice(i, "buffer overrun [cnt: %ld]",
			   bbi->overruns);
	};

	/* write */
	hd=lfds611_freelist_get_user_data_from_element(elem, NULL);

	len = data_size(msg);
	memcpy(hd->data, msg->data, len);
	hd->data_len=msg->len;

	ubx_debug(i, "copying %ld bytes", len);

	/* release element */
	lfds611_ringbuffer_put_write_element(bbi->rbs, elem);

 out:
	return;
}

/* where to check whether the msg->data len is long enough? */
static int cyclic_read(ubx_block_t *i, ubx_data_t* msg)
{
	int ret = 0;
	unsigned long readlen, readsz;
	struct cyclic_block_info *bbi;
	struct lfds611_freelist_element *elem;
	struct cyclic_elem_header *hd;

	bbi = (struct cyclic_block_info*) i->private_data;

	if(bbi->type != msg->type) {
		ubx_err(i, "invalid message type %s", msg->type->name);
		goto out;
	}

	if(lfds611_ringbuffer_get_read_element(bbi->rbs, &elem)==NULL) {
		ret=0;
		goto out;
	}

	hd=lfds611_freelist_get_user_data_from_element(elem, NULL);

	if(msg->len < hd->data_len) {
		ubx_err(i, "only copying %lu array elements of %lu",
			msg->len, hd->data_len);
	}

	readlen = MIN(msg->len, hd->data_len);
	readsz = bbi->type->size * readlen;

	ubx_debug("%s: copying %ld bytes", i->name, readsz);

	memcpy(msg->data, hd->data, readsz);
	lfds611_ringbuffer_put_read_element(bbi->rbs, elem);
	ret=readlen; /* compute number of elements read */
 out:
	return ret;
}

/* put everything together */
ubx_block_t cyclic_comp = {
	.name = "lfds_buffers/cyclic",
	.type = BLOCK_TYPE_INTERACTION,
	.meta_data = cyclic_meta,
	.configs = cyclic_config,
	.ports = cyclic_ports,

	.init=cyclic_init,
	.cleanup=cyclic_cleanup,

	/* iops */
	.write=cyclic_write,
	.read=cyclic_read,
};

static int cyclic_mod_init(ubx_node_info_t* ni)
{
	DBG(" ");
	return ubx_block_register(ni, &cyclic_comp);
}

static void cyclic_mod_cleanup(ubx_node_info_t *ni)
{
	DBG(" ");
	ubx_block_unregister(ni, "lfds_buffers/cyclic");
}

UBX_MODULE_INIT(cyclic_mod_init)
UBX_MODULE_CLEANUP(cyclic_mod_cleanup)
