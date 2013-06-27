/*
 * A lock-free interaction
 */

#define DEBUG 1

#include <stdio.h>
#include <stdlib.h>

#include "liblfds611/inc/liblfds611.h"

#include "u5c.h"

/* meta-data */
char cyclic_meta[] =
	"{ doc='High perforance scalable, lock-free cyclic, buffered in process communication"
	"  description=[[This u5c interaction is based on based on liblfds"
	"                ringbuffer (v0.6.1.1) (www.liblfds.org)]],"
	"  version=0.01,"
	"  license='MIT',"
	"  hard_real_time=true,"
	"}";

/* configuration */
u5c_config_t cyclic_config[] = {
	{ .name="cyclic_num", .type_name = "uint32_t" },
	{ .name="cyclic_size", .type_name = "uint32_t" },
	{ NULL },
};

/* interaction private data */
struct cyclic_block_info {
	unsigned long num;		/* number of elements */
	unsigned long size;		/* buffer size of each element */
	const u5c_type_t* type;		/* type of contained elements */
	struct lfds611_ringbuffer_state *rbs;

	unsigned long overruns;		/* stats */
};

struct cyclic_elem_header {
	unsigned long len;
	uint8_t data[0];
};


int cyclic_data_elem_init(void **user_data, void *user_state)
{
	DBG("allocating stuff");
	struct cyclic_block_info* bbi = (struct cyclic_block_info*) user_state;
	*user_data=calloc(1, bbi->size+sizeof(struct cyclic_elem_header));
	return (*user_data==NULL) ? 0 : 1;
}

void cyclic_data_elem_del(void *user_data, void *user_state)
{
	free(user_data);
}

/* init */
static int cyclic_init(u5c_block_t *i)
{
	int ret = -1;
	unsigned int len;
	struct cyclic_block_info* bbi;

	if((i->private_data = calloc(1, sizeof(struct cyclic_block_info)))==NULL) {
		ERR("failed to alloc cyclic_block_info");
		goto out;
	}

	bbi = (struct cyclic_block_info*) i->private_data;

	/* read/check configuration */
	bbi->num = *((uint32_t*) u5c_config_get_data_ptr(i, "cyclic_num", &len));
	if(bbi->num==0) {
		bbi->num=4; /* goto out; */
		ERR("invalid number of elements 0, setting to %ld TODO: FIXME!", bbi->num);
	}

	bbi->size = *((uint32_t*) u5c_config_get_data_ptr(i, "cyclic_size", &len));
	if(bbi->size==0) {
		bbi->size=4; /* goto out; */
		ERR("invalid config cyclic_size 0, setting to %ld TODO: FIXME!", bbi->size);
	}

	DBG("allocating ringbuffer with %lu elements of size %lu bytes.", bbi->num, bbi->size);
	if(lfds611_ringbuffer_new(&bbi->rbs, bbi->num, cyclic_data_elem_init, bbi)==0) {
		ERR("%s: creating ringbuffer 0x%ld x 0x%ld bytes failed",
		    i->name, bbi->num, bbi->size);
		goto out_free_priv_data;
	}
	DBG("allocated ringbuffer");
	ret=0;
	goto out;

 out_free_priv_data:
	free(i->private_data);
 out:
	return ret;
};

/* cleanup */
static void cyclic_cleanup(u5c_block_t *i)
{
	struct cyclic_block_info *bbi;
	bbi = (struct cyclic_block_info*) i->private_data;
	lfds611_ringbuffer_delete(bbi->rbs, cyclic_data_elem_del, bbi);
	free(bbi);
}

/* write */
static void cyclic_write(u5c_block_t *i, u5c_data_t* msg)
{
	int ret;
	long len;
	struct cyclic_block_info *bbi;
	struct lfds611_freelist_element *elem;
	struct cyclic_elem_header *hd;

	bbi = (struct cyclic_block_info*) i->private_data;

	len = data_len(msg);

	if (len > bbi->size) {
		ERR("can't store %ld bytes of data in a %ld size buffer", len, bbi->size);
		goto out;
	}

	/* remember type, this should better happen in preconnect-hook. */
	if(bbi->type==NULL) {
		DBG("SETTING type to msg->type = %p, %s", msg->type, msg->type->name);
		bbi->type=msg->type;
	}

	elem = lfds611_ringbuffer_get_write_element(bbi->rbs, &elem, &ret);

	if(ret) {
		bbi->overruns++;
		DBG("buffer overrun (#%ld), overwriting old data.", bbi->overruns);
	};

	/* write */
	hd=lfds611_freelist_get_user_data_from_element(elem, NULL);
	memcpy(hd->data, msg->data, len);
	hd->len=len;

	/* release element */
	lfds611_ringbuffer_put_write_element(bbi->rbs, elem);

 out:
	return;
}

/* where to check whether the msg->data len is long enough? */
static int cyclic_read(u5c_block_t *i, u5c_data_t* msg)
{
	int ret;
	unsigned long readsz;
	struct cyclic_block_info *bbi;
	struct lfds611_freelist_element *elem;
	struct cyclic_elem_header *hd;

	bbi = (struct cyclic_block_info*) i->private_data;

	if(lfds611_ringbuffer_get_read_element(bbi->rbs, &elem)==NULL) {
		ret=PORT_READ_NODATA;
		goto out;
	}

	hd=lfds611_freelist_get_user_data_from_element(elem, NULL);

	readsz=data_len(msg);
	readsz=MIN(readsz, hd->len);
	DBG("copying %ld bytes", readsz);

	memcpy(msg->data, hd->data, readsz);
	lfds611_ringbuffer_put_read_element(bbi->rbs, elem);
	ret=readsz/msg->type->size;		/* compute number of elements read */
 out:
	return ret;
}

/* put everything together */
u5c_block_t cyclic_comp = {
	.name = "lfds_cyclic",
	.type = BLOCK_TYPE_INTERACTION,
	.meta_data = cyclic_meta,
	.configs = cyclic_config,

	.init=cyclic_init,
	.cleanup=cyclic_cleanup,

	/* iops */
	.write=cyclic_write,
	.read=cyclic_read,
};

static int cyclic_mod_init(u5c_node_info_t* ni)
{
	DBG(" ");
	return u5c_block_register(ni, &cyclic_comp);
}

static void cyclic_mod_cleanup(u5c_node_info_t *ni)
{
	DBG(" ");
	u5c_block_unregister(ni, BLOCK_TYPE_INTERACTION, "lfds_cyclic");
}

module_init(cyclic_mod_init)
module_cleanup(cyclic_mod_cleanup)
