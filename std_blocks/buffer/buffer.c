/*
 * A buffered interaction block
 */

#define DEBUG 1

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "u5c.h"

/* meta-data */
char buffermeta[] =
	"{ doc='a buffered, in-process interaction.',"
	"  license='MIT',"
	"  real-time=false,"
	"}";

/* configuration */
u5c_config_t buffer_config[] = {
	{ .name="buffer_size", .type_name = "uint32_t" },
	{ NULL },
};

/* interaction private data */
struct buffer_block_info {
	int mode;	 		/* circular, ... */
	unsigned long size;		/* size in bytes */
	uint8_t *buff, *buff_cur;
	const u5c_type_t* type;		/* the type handled, set on first write */
	pthread_mutex_t mutex; 		/* naive mutex implementation */
	unsigned long overruns;		/* stats */
};


/* init */
static int buffer_init(u5c_block_t *i)
{
	int ret = -1;
	struct buffer_block_info* bbi;

	if((i->private_data = calloc(1, sizeof(struct buffer_block_info)))==NULL) {
		ERR("failed to alloc buffer_block_info");
		goto out;
	}

	bbi = (struct buffer_block_info*) i->private_data;
	pthread_mutex_init(&bbi->mutex, NULL);

	bbi->size = *((uint32_t*) u5c_config_get_data(i, "buffer_size"));
	
	if(bbi->size==0) {
		/* goto out; */
		bbi->size=16;
		ERR("invalid buffersize 0, setting to %ld TODO: FIXME!", bbi->size);
	}

	if((bbi->buff=malloc(bbi->size))==NULL) {
		ERR("failed to allocate buffer");
		goto out_free_priv_data;
	}

	/* set to empty */
	bbi->buff_cur = bbi->buff;
	bbi->type=NULL;

	DBG("allocated buffer of size %ld", bbi->size);
	ret=0;
	goto out;

 out_free_priv_data:
	free(i->private_data);
 out:
	return ret;
};

/* cleanup */
static void buffer_cleanup(u5c_block_t *i)
{
	struct buffer_block_info *bbi;
	bbi = (struct buffer_block_info*) i->private_data;

	free(bbi->buff);
	free(bbi);
}

/* write */
static void buffer_write(u5c_block_t *i, u5c_data_t* msg)
{
	int ret;
	unsigned long len;
	struct buffer_block_info *bbi;

	bbi = (struct buffer_block_info*) i->private_data;

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
		DBG("buffer overrun (#%ld) dropping data.", bbi->overruns);
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
static int buffer_read(u5c_block_t *i, u5c_data_t* msg)
{
	int ret;
	unsigned long readsz, buf_used;
	struct buffer_block_info *bbi;

	bbi = (struct buffer_block_info*) i->private_data;

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
	buf_used = bbi->buff_cur - bbi->buff;	/* size of filled buffer */
	readsz = MIN(data_len(msg), buf_used);	/* size to read out */
	bbi->buff_cur-=readsz;			/* update buffer ptr */
	memcpy(msg->data, bbi->buff_cur, readsz);
	ret=readsz/bbi->type->size;		/* compute # elements read */
 out_unlock:
	pthread_mutex_unlock(&bbi->mutex);
 out:
	return ret;
}

/* put everything together */
u5c_block_t buffer_comp = {
	.name = "buffer",
	.type = BLOCK_TYPE_INTERACTION,
	.meta_data = buffermeta,
	.configs = buffer_config,

	.init=buffer_init,
	.cleanup=buffer_cleanup,

	/* iops */
	.write=buffer_write,
	.read=buffer_read,
};

static int buffer_mod_init(u5c_node_info_t* ni)
{
	DBG(" ");	
	return u5c_block_register(ni, &buffer_comp);
}

static void buffer_mod_cleanup(u5c_node_info_t *ni)
{
	DBG(" ");
	u5c_block_unregister(ni, BLOCK_TYPE_INTERACTION, "buffer");
}

module_init(buffer_mod_init)
module_cleanup(buffer_mod_cleanup)
