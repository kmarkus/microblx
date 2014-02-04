/*
 * A lock-free interaction
 */

/* #define DEBUG 1 */

#include <stdio.h>
#include <stdlib.h>

#include "liblfds611/inc/liblfds611.h"

#include "ubx.h"

UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)

/* meta-data */
char cyclic_meta[] =
	"{ doc='High performance scalable, lock-free cyclic, buffered in process communication"
	"  description=[[This ubx interaction is based on based on liblfds"
	"                ringbuffer (v0.6.1.1) (www.liblfds.org)]],"
	"  version=0.01,"
	"  hard_real_time=true,"
	"}";

/* configuration */
ubx_config_t cyclic_config[] = {
	{ .name="element_num", .type_name = "uint32_t" },
	{ .name="element_size", .type_name = "uint32_t" },
	{ NULL },
};

ubx_port_t cyclic_ports[] = {
	/* generic arm+base */
	{ .name="overruns", .out_type_name="unsigned long" },
	{ NULL },
};

/* interaction private data */
struct cyclic_block_info {
	unsigned long num;		/* number of elements */
	unsigned long size;		/* buffer size of each element */
	const ubx_type_t* type;		/* type of contained elements */
	struct lfds611_ringbuffer_state *rbs;

	unsigned long overruns;		/* stats */
	ubx_port_t* p_overruns;
};

struct cyclic_elem_header {
	unsigned long len;
	uint8_t data[0];
};

def_write_fun(write_ulong, unsigned long)

int cyclic_data_elem_init(void **user_data, void *user_state)
{
	struct cyclic_block_info* bbi = (struct cyclic_block_info*) user_state;
	*user_data=calloc(1, bbi->size+sizeof(struct cyclic_elem_header));
	return (*user_data==NULL) ? 0 : 1;
}

void cyclic_data_elem_del(void *user_data, void *user_state)
{
	free(user_data);
}


/* init */
static int cyclic_init(ubx_block_t *i)
{
	int ret = -1;
	unsigned int len;
	struct cyclic_block_info* bbi;

	if((i->private_data = calloc(1, sizeof(struct cyclic_block_info)))==NULL) {
		ERR("failed to alloc cyclic_block_info");
		ret = EOUTOFMEM;
		goto out;
	}

	bbi = (struct cyclic_block_info*) i->private_data;

	/* read/check configuration */
	bbi->num = *((uint32_t*) ubx_config_get_data_ptr(i, "element_num", &len));
	if(bbi->num==0) {
		ERR("invalid configuration element_num=0");
		ret = EINVALID_CONFIG;
		goto out_free_priv_data;
	}

	bbi->size = *((uint32_t*) ubx_config_get_data_ptr(i, "element_size", &len));
	if(bbi->size==0) {
		ERR("invalid configuration cyclic_size 0");
		ret = EINVALID_CONFIG;
		goto out_free_priv_data;
	}

	DBG("allocating ringbuffer with %lu elements of size %lu bytes.", bbi->num, bbi->size);
	if(lfds611_ringbuffer_new(&bbi->rbs, bbi->num, cyclic_data_elem_init, bbi)==0) {
		ERR("%s: creating ringbuffer 0x%ld x 0x%ld bytes failed", i->name, bbi->num, bbi->size);
		ret = EOUTOFMEM;
		goto out_free_priv_data;
	}

	/* cache port ptrs */
	assert(bbi->p_overruns = ubx_port_get(i, "overruns"));

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

	len = data_size(msg);

	if (len > bbi->size) {
		ERR("can't store %ld bytes of data in a %ld size buffer", len, bbi->size);
		goto out;
	}

	/* remember type, this should better happen in preconnect-hook. */
	if(bbi->type==NULL) {
		DBG("%s: SETTING type to msg->type = %p, %s", i->name, msg->type, msg->type->name);
		bbi->type=msg->type;
	}

	elem = lfds611_ringbuffer_get_write_element(bbi->rbs, &elem, &ret);

	if(ret) {
		bbi->overruns++;
		write_ulong(bbi->p_overruns, &bbi->overruns);
		DBG("%s: buffer overrun (#%ld), overwriting old data.", i->name, bbi->overruns);
	};

	/* write */
	hd=lfds611_freelist_get_user_data_from_element(elem, NULL);
	memcpy(hd->data, msg->data, len);
	hd->len=len;
	DBG("%s: copying %ld bytes", i->name, len);

	/* release element */
	lfds611_ringbuffer_put_write_element(bbi->rbs, elem);

 out:
	return;
}

/* where to check whether the msg->data len is long enough? */
static int cyclic_read(ubx_block_t *i, ubx_data_t* msg)
{
	int ret;
	unsigned long readsz;
	struct cyclic_block_info *bbi;
	struct lfds611_freelist_element *elem;
	struct cyclic_elem_header *hd;

	bbi = (struct cyclic_block_info*) i->private_data;

	if(lfds611_ringbuffer_get_read_element(bbi->rbs, &elem)==NULL) {
		ret=0;
		goto out;
	}

	hd=lfds611_freelist_get_user_data_from_element(elem, NULL);

	readsz=data_size(msg);

	if(readsz < hd->len)
		ERR("%s: warning: only copying %lu of %lu bytes", i->name, readsz, hd->len);

	readsz=MIN(readsz, hd->len);
	DBG("%s: copying %ld bytes", i->name, readsz);

	memcpy(msg->data, hd->data, readsz);
	lfds611_ringbuffer_put_read_element(bbi->rbs, elem);
	ret=readsz/msg->type->size;		/* compute number of elements read */
 out:
	return ret;
}

/* put everything together */
ubx_block_t cyclic_comp = {
	.name = "lfds_buffers/cyclic_raw",
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
	ubx_block_unregister(ni, "lfds_buffers/cyclic_raw");
}

UBX_MODULE_INIT(cyclic_mod_init)
UBX_MODULE_CLEANUP(cyclic_mod_cleanup)
