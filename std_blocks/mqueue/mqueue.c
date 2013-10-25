/*
 * An interaction block that sends data via a mqueue
 */

#define DEBUG 1

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <mqueue.h>

#include "ubx.h"

char mqueue_meta[] =
	"{ doc='POSIX mqueue interaction',"
	"  license='MIT',"
	"  real-time=true,"
	"}";

ubx_config_t mqueue_config[] = {
	{ .name="mq_name", .type_name = "char" },
	{ .name="buffer_size", .type_name = "uint32_t" },
	{ NULL }
};

struct mqueue_msg {
	uint8_t type_hash[TYPE_HASH_LEN_UNIQUE];
	uint32_t data_size;
	uint8_t data[0];
};

struct mqueue_info {
	mqd_t mqd;
	char *mq_name;
	uint32_t buffer_size;
	struct mqueue_msg *msg;
	
	unsigned long cnt_send_err;
	unsigned long cnt_recv_err;
};


static int mqueue_init(ubx_block_t *i)
{
	int ret=-1;
	unsigned int tmplen;
	char* chrptr;

	struct mqueue_info* mqi;

	if((i->private_data = calloc(1, sizeof(struct mqueue_info)))==NULL) {
		ERR("failed to alloc mqueue_info");
		ret = EOUTOFMEM;
		goto out;
	}

	mqi = (struct mqueue_info*) i->private_data;

	/* retrive mq_name config */
	chrptr = (char*) ubx_config_get_data_ptr(i, "mq_name", &tmplen);
	
	if(strcmp(chrptr, "")==0) {
		ERR("%s: mq_name is empty", i->name);
		goto out_free_info;
	}

	mqi->mq_name = strdup(chrptr);

	/* get buffer_size config */
	mqi->buffer_size = *((uint32_t*) ubx_config_get_data_ptr(i, "buffer_size", &tmplen));

	if (mqi->buffer_size <= 0) {
		ERR("%s: invalid value for config buffer_size: %d", i->name, mqi->buffer_size);
		goto out_free_mq_name;
	}
	
	/* allocate message buffer */
	mqi->msg = (struct mqueue_msg*) malloc(sizeof(struct mqueue_msg) + mqi->buffer_size);

	if(mqi->msg==NULL) {
		ERR("%s: failed to allocate buffer of size %d", i->name, mqi->buffer_size);
		goto out;
	}
		

	mqi->mqd = mq_open(mqi->mq_name, O_RDWR | O_CREAT | O_NONBLOCK, S_IRUSR | S_IWUSR, NULL);

	ret=0;
	goto out;

 out_free_mq_name:
	free(mqi->mq_name);
 out_free_info:
	free(mqi);
 out:
	return ret;
}

static void mqueue_cleanup(ubx_block_t *i)
{
	int ret;
	struct mqueue_info* mqi = (struct mqueue_info*) i->private_data;

	ret = mq_unlink(mqi->mq_name);

	if( ret < 0 && errno !=ENOENT) {
		ERR2(errno, "Failed to unlink message queue %s", mqi->mq_name);
	}
	
	free(mqi->mq_name);
	free(mqi);
}

static int mqueue_read(ubx_block_t *i, ubx_data_t* data)
{
	/* TODO */
	/* struct mqueue_info* mqi; */
	/*  mqi = (struct mqueue_info*) i->private_data; */
	return 0;
}

static void mqueue_write(ubx_block_t *i, ubx_data_t* data) 
{
	int ret, size;
	struct mqueue_info* mqi;

	mqi = (struct mqueue_info*) i->private_data;

	size = data_size(data);

	if(size > mqi->buffer_size) {
		ERR("%s: message too larger than buffer_size (%d), dropping", i->name, mqi->buffer_size);
		goto out;
	}

	size = MAX(size, mqi->buffer_size);
	
	memcpy(mqi->msg->type_hash, data->type->hash, TYPE_HASH_LEN_UNIQUE);
	memcpy(mqi->msg->data, data->data, size);
	mqi->msg->data_size = size;
	
	/* TODO: prio config or/and port */
	ret=mq_send(mqi->mqd, (const char*) mqi->msg, mqi->msg->data_size + sizeof(struct mqueue_msg), 1);
	
	if(ret != 0) {
		ERR2(errno, "%s: sending message failed", i->name);
		mqi->cnt_send_err++;
		goto out;
	};

 out:
	return;
}

/* put everything together */
ubx_block_t mqueue_comp = {
	.name = "mqueue",
	.type = BLOCK_TYPE_INTERACTION,
	.meta_data = mqueue_meta,
	.configs = mqueue_config,
	
	/* ops */
	.init = mqueue_init,
	.cleanup = mqueue_cleanup,
	.read=mqueue_read,
	.write=mqueue_write,

};

static int mqueue_mod_init(ubx_node_info_t* ni)
{
	DBG(" ");	
	return ubx_block_register(ni, &mqueue_comp);
}

static void mqueue_mod_cleanup(ubx_node_info_t *ni)
{
	DBG(" ");
	ubx_block_unregister(ni, "mqueue");
}

UBX_MODULE_INIT(mqueue_mod_init)
UBX_MODULE_CLEANUP(mqueue_mod_cleanup)
