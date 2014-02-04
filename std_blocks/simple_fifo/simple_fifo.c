/*
 * A fifoed interaction block
 */

#define DEBUG 1

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "ubx.h"

/* meta-data */
char fifometa[] =
	"{ doc='a FIFO buffered, in-process interaction.',"
	"  description=[[This FIFO interaction serves as an example interaction."
	"               Although functional, it does not scale well to multiple"
	"               reader/writers. For that case, the lfds_* versions should"
	"               be preferred.]],"
	"  version=0.01,"
	"  real-time=false,"
	"}";

/* configuration */
ubx_config_t fifo_config[] = {
	{ .name="fifo_size", .type_name = "uint32_t" },
	{ .name="overrun_policy", .type_name = "uint32_t" },
	{ NULL },
};

/* interaction private data */
struct fifo_block_info {
	int mode;	 		/* circular, ... */
	unsigned long size;		/* size in bytes */
	uint8_t *buff;
	uint8_t *rdptr;
	uint8_t *wrptr;
	const ubx_type_t* type;		/* the type handled, set on first write */
	uint32_t overrun_policy;
	pthread_mutex_t mutex; 		/* naive mutex implementation */
	unsigned long overruns;		/* stats */
};

enum {
	DROP_NEW=1,
	DROP_OLD=2,
};

/* init */
static int fifo_init(ubx_block_t *i)
{
	int ret = -1;
	struct fifo_block_info* bbi;

	if((i->private_data = calloc(1, sizeof(struct fifo_block_info)))==NULL) {
		ERR("failed to alloc fifo_block_info");
		goto out;
	}

	bbi = (struct fifo_block_info*) i->private_data;
	pthread_mutex_init(&bbi->mutex, NULL);

	bbi->size = *((uint32_t*) ubx_config_get_data(i, "fifo_size"));

	if(bbi->size==0) {
		/* goto out; */
		bbi->size=16;
		ERR("invalid fifosize 0, setting to %ld TODO: FIXME!", bbi->size);
	}

	if((bbi->buff=malloc(bbi->size))==NULL) {
		ERR("failed to allocate fifo");
		goto out_free_priv_data;
	}

	/* set to empty */
	bbi->rdptr = bbi->wrptr = bbi->buff;
	bbi->type=NULL;

	DBG("allocated fifo of size %ld", bbi->size);
	ret=0;
	goto out;

 out_free_priv_data:
	free(i->private_data);
 out:
	return ret;
};

/* cleanup */
static void fifo_cleanup(ubx_block_t *i)
{
	struct fifo_block_info *bbi;
	bbi = (struct fifo_block_info*) i->private_data;

	free(bbi->buff);
	free(bbi);
}

#define empty_space(bbi)						\
	((bbi->wrptr >= bbi->rdptr)					\
	 ? (bbi->size - (bbi->wrptr - bbi->rdptr))			\
	 : (bbi->rdptr - bbi->wrptr))

#define used_space(bbi)					\
	((bbi->wrptr < bbi->rdptr)			\
	 ? (bbi->size - (bbi->rdptr - bbi->wrptr))	\
	 : (bbi->wrptr - bbi->rdptr))

/* write */
static void fifo_write(ubx_block_t *i, ubx_data_t* msg)
{
	int ret;
	long len, empty, len2=0;
	struct fifo_block_info *bbi;

	bbi = (struct fifo_block_info*) i->private_data;

	if((ret=pthread_mutex_lock(&bbi->mutex))!=0) {
		ERR2(ret, "failed to lock mutex");
		goto out;
	}

	len = data_size(msg);

	if (len > bbi->size) {
		ERR("can't store %ld bytes of data in a %ld size buffer", len, bbi->size);
		goto out_unlock;
	}

	/* remember type, this should better happen in preconnect-hook. */
	if(bbi->type==NULL) {
		DBG("SETTING type to msg->type = %p, %s", msg->type, msg->type->name);
		bbi->type=msg->type;
	}

	/* enough space?
	 * TODO: continue here:
	 *  - add an overrun policy: 'drop' or 'overwrite'
	 */
	empty = empty_space(bbi);

	if(empty < len) {
		bbi->overruns++;

		if(bbi->overrun_policy==DROP_NEW) {
			DBG("fifo overrun (#%ld), dropping new data.", bbi->overruns);
			goto out_unlock;
		} else if(bbi->overrun_policy==DROP_OLD) {
			/* fake a read of len and deal with wrapping */
			bbi->rdptr+=len;
			if(bbi->rdptr > bbi->buff+bbi->size)
				bbi->rdptr-=bbi->size;
			DBG("fifo overrun (#%ld), dropping old data.", bbi->overruns);
		} else {
			ERR("unknown overrun_policy 0x%x", bbi->overrun_policy);
			goto out_unlock;
		}
	}

	/* once we get here, there is enough free space.
	 * two possible cases:
	 *    1. continous free space available for write of len
	 *    2. non-continous, need to wrap around to write chunk 2.
	 */

	/* compute length of chunk 2 write */
	if(bbi->wrptr > bbi->rdptr) {
		len2 = len - (bbi->buff + bbi->size - bbi->wrptr);
		len2 = (len2>0) ? len2 : 0;
		len = (len2>0) ? len-len2 : len;
	}

	DBG("empty=%ld, len=%ld, len2=%ld\n",empty,len,len2);

	/* chunk 1 */
	memcpy(bbi->wrptr, msg->data, len);
	bbi->wrptr = (bbi->wrptr+len >= bbi->buff+bbi->size) ? bbi->buff : &bbi->wrptr[len];
	

	/* chunk 2 ?*/
	if(len2 > 0) {
		memcpy(bbi->buff, &(((uint8_t*) msg->data)[len]), len2);
		bbi->wrptr=&bbi->buff[len2];
	}



 out_unlock:
	pthread_mutex_unlock(&bbi->mutex);
 out:
	return;
}

/* read */
static int fifo_read(ubx_block_t *i, ubx_data_t* msg)
{
	int ret=0;
	unsigned long readsz=0, readsz1=0, readsz2=0, used;
	struct fifo_block_info *bbi;

	bbi = (struct fifo_block_info*) i->private_data;

	if((ret=pthread_mutex_lock(&bbi->mutex))!=0) {
		ERR2(ret, "failed to lock mutex");
		goto out;
	}

	if(bbi->rdptr == bbi->wrptr) {
		goto out_unlock;
	}

	if(msg->type != bbi->type) {
		ERR("invalid read type '%s' (expected '%s'", get_typename(msg), bbi->type->name);
		ret=EPORT_INVALID_TYPE;
		goto out_unlock;
	}

	/* bytes */
	used = used_space(bbi);
	readsz=(used<readsz) ? used : readsz;

	/* chunk 2*/
	if(bbi->rdptr > bbi->wrptr) {
		readsz2 = readsz - (bbi->buff + bbi->size - bbi->wrptr);
		readsz1 = readsz-readsz2;
	}

	memcpy(msg->data, bbi->rdptr, readsz1);
	bbi->rdptr=&bbi->buff[readsz1];

	if(readsz2>0) {
		
	}

	ret=readsz/bbi->type->size;		/* compute # elements read */
 out_unlock:
	pthread_mutex_unlock(&bbi->mutex);
 out:
	return ret;
}

/* put everything together */
ubx_block_t fifo_comp = {
	.name = "examples/simple_fifo",
	.type = BLOCK_TYPE_INTERACTION,
	.meta_data = fifometa,
	.configs = fifo_config,

	.init=fifo_init,
	.cleanup=fifo_cleanup,

	/* iops */
	.write=fifo_write,
	.read=fifo_read,
};

static int fifo_mod_init(ubx_node_info_t* ni)
{
	DBG(" ");
	return ubx_block_register(ni, &fifo_comp);
}

static void fifo_mod_cleanup(ubx_node_info_t *ni)
{
	DBG(" ");
	ubx_block_unregister(ni, "examples/simple_fifo");
}

UBX_MODULE_INIT(fifo_mod_init)
UBX_MODULE_CLEANUP(fifo_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
