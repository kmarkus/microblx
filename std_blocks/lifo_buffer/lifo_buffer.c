/*
 * A lifoed interaction block
 */

#define DEBUG 1

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "u5c.h"

/* meta-data */
char lifometa[] =
	"{ doc='a last-in, first out (stack), in-process interaction.',"
	"  license='MIT',"
	"  real-time=false,"
	"}";

/* configuration */
u5c_config_t lifo_config[] = {
	{ .name="lifo_size", .type_name = "uint32_t" },
	{ NULL },
};

/* interaction private data */
struct lifo_block_info {
	int mode;	 		/* circular, ... */
	unsigned long size;		/* size in bytes */
	uint8_t *buff, *buff_cur;
	const u5c_type_t* type;		/* the type handled, set on first write */
	pthread_mutex_t mutex; 		/* naive mutex implementation */
	unsigned long overruns;		/* stats */
};


/* init */
static int lifo_init(u5c_block_t *i)
{
	int ret = -1;
	struct lifo_block_info* bbi;

	if((i->private_data = calloc(1, sizeof(struct lifo_block_info)))==NULL) {
		ERR("failed to alloc lifo_block_info");
		goto out;
	}

	bbi = (struct lifo_block_info*) i->private_data;
	pthread_mutex_init(&bbi->mutex, NULL);

	bbi->size = *((uint32_t*) u5c_config_get_data(i, "lifo_size"));
	
	if(bbi->size==0) {
		/* goto out; */
		bbi->size=16;
		ERR("invalid lifosize 0, setting to %ld TODO: FIXME!", bbi->size);
	}

	if((bbi->buff=malloc(bbi->size))==NULL) {
		ERR("failed to allocate lifo");
		goto out_free_priv_data;
	}

	/* set to empty */
	bbi->buff_cur = bbi->buff;
	bbi->type=NULL;

	DBG("allocated lifo of size %ld", bbi->size);
	ret=0;
	goto out;

 out_free_priv_data:
	free(i->private_data);
 out:
	return ret;
};

/* cleanup */
static void lifo_cleanup(u5c_block_t *i)
{
	struct lifo_block_info *bbi;
	bbi = (struct lifo_block_info*) i->private_data;

	free(bbi->buff);
	free(bbi);
}

/* write */
static void lifo_write(u5c_block_t *i, u5c_data_t* msg)
{
	int ret;
	unsigned long len;
	struct lifo_block_info *bbi;

	bbi = (struct lifo_block_info*) i->private_data;

	if((ret=pthread_mutex_lock(&bbi->mutex))!=0) {
		ERR2(ret, "failed to lock mutex");
		goto out;
	}

	/* remember type, this should better happen in preconnect-hook. */
	if(bbi->type==NULL) {
		DBG("SETTING type to msg->type = %p, %s", msg->type, msg->type->name);
		bbi->type=msg->type;
	}

	len = data_len(msg);

	/* enough space? */
	if(bbi->size - (bbi->buff_cur - bbi->buff) < len) {
		bbi->overruns++;
		DBG("lifo overrun (#%ld) dropping data.", bbi->overruns);
		goto out_unlock;
	}

	memcpy(bbi->buff_cur, msg->data, len);
	bbi->buff_cur+=len;

 out_unlock:
	pthread_mutex_unlock(&bbi->mutex);
 out:
	return;
}

/* read */
static int lifo_read(u5c_block_t *i, u5c_data_t* msg)
{
	int ret;
	unsigned long readsz, buf_used;
	struct lifo_block_info *bbi;

	bbi = (struct lifo_block_info*) i->private_data;

	if((ret=pthread_mutex_lock(&bbi->mutex))!=0) {
		ERR2(ret, "failed to lock mutex");
		goto out;
	}

	if(bbi->buff == bbi->buff_cur) {
		ret=PORT_READ_NODATA;
		goto out_unlock;
	}

	if(msg->type != bbi->type) {
		ERR("invalid read type '%s' (expected '%s'", get_typename(msg), bbi->type->name);
		ret=EPORT_INVALID_TYPE;
		goto out_unlock;
	}

	/* bytes */
	buf_used = bbi->buff_cur - bbi->buff;	/* size of filled lifo */
	readsz = MIN(data_len(msg), buf_used);	/* size to read out */
	bbi->buff_cur-=readsz;			/* update lifo ptr */
	memcpy(msg->data, bbi->buff_cur, readsz);
	ret=readsz/bbi->type->size;		/* compute # elements read */
 out_unlock:
	pthread_mutex_unlock(&bbi->mutex);
 out:
	return ret;
}

/* put everything together */
u5c_block_t lifo_comp = {
	.name = "lifo",
	.type = BLOCK_TYPE_INTERACTION,
	.meta_data = lifometa,
	.configs = lifo_config,

	.init=lifo_init,
	.cleanup=lifo_cleanup,

	/* iops */
	.write=lifo_write,
	.read=lifo_read,
};

static int lifo_mod_init(u5c_node_info_t* ni)
{
	DBG(" ");	
	return u5c_block_register(ni, &lifo_comp);
}

static void lifo_mod_cleanup(u5c_node_info_t *ni)
{
	DBG(" ");
	u5c_block_unregister(ni, BLOCK_TYPE_INTERACTION, "lifo");
}

module_init(lifo_mod_init)
module_cleanup(lifo_mod_cleanup)
