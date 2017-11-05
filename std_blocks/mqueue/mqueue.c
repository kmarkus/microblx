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
	"  real-time=true,"
	"}";

ubx_config_t mqueue_config[] = {
	{ .name="mq_name", .type_name = "char" },
	{ .name="type_name", .type_name = "char", .doc="name of registered microblx type to transport" },
	{ .name="data_len", .type_name = "uint32_t", .doc="array length (multiplier) of data (default: 1)" },
	{ .name="buffer_len", .type_name = "uint32_t", .doc="max. number of data elements the buffer shall hold" },
	{ NULL }
};

struct mqueue_info {
	mqd_t mqd;
	char *mq_name;
	struct mqueue_msg *msg;

	const ubx_type_t* type;		/* type of contained elements */
	unsigned long data_len;		/* buffer size of each element */
	unsigned long buffer_len;	/* number of elements */

	unsigned long cnt_send_err;
	unsigned long cnt_recv_err;
};


static int mqueue_init(ubx_block_t *i)
{
	int ret=-1;
	unsigned int tmplen;
	const char* chrptr;

	struct mqueue_info* mqi;
	struct mq_attr mqa;

	if((i->private_data = calloc(1, sizeof(struct mqueue_info)))==NULL) {
		ERR("failed to alloc mqueue_info");
		ret = EOUTOFMEM;
		goto out;
	}

	mqi = (struct mqueue_info*) i->private_data;

	/* config buffer_len */
	mqi->buffer_len = *((unsigned long*) ubx_config_get_data_ptr(i, "buffer_len", &tmplen));
	if(mqi->buffer_len==0) {
		ERR("invalid configuration buffer_len=0");
		ret = EINVALID_CONFIG;
		goto out_free_info;
	}
	mqa.mq_maxmsg = mqi->buffer_len;

	/* config data_len */
	mqi->data_len = *((uint32_t*) ubx_config_get_data_ptr(i, "data_len", &tmplen));
	mqi->data_len = (mqi->data_len == 0) ? 1 : mqi->data_len;

	/* config type_name */
	chrptr = (const char*) ubx_config_get_data_ptr(i, "type_name", &tmplen);

	if (chrptr == NULL || tmplen <= 0) {
		ERR("%s: invalid or missing type name", i->name);
		goto out_free_info;
	}

	mqi->type=ubx_type_get(i->ni, chrptr);

	/* configure max message size */
	mqa.mq_msgsize = mqi->data_len * mqi->type->size;

	if(mqi->type==NULL) {
		ERR("%s: failed to lookup type %s", i->name, chrptr);
		ret = EINVALID_CONFIG;
		goto out_free_info;
	}

	/* retrive mq_name config */
	chrptr = (char*) ubx_config_get_data_ptr(i, "mq_name", &tmplen);

	if(strcmp(chrptr, "")==0) {
		ERR("%s: mq_name is empty", i->name);
		goto out_free_info;
	}

	if(chrptr[0] != '/') {
		ERR("%s: invalid mq_name, must start with '/' (cfr. mq_overview(1))", i->name);
		goto out_free_info;
	}

	mqi->mq_name = strdup(chrptr);



	if (mqa.mq_msgsize <= 0) {
		ERR("%s: invalid value for mq_msgsize: %ld", i->name, mqa.mq_msgsize);
		goto out_free_mq_name;
	}

	mqa.mq_flags = O_NONBLOCK;

	mqi->mqd = mq_open(mqi->mq_name, O_RDWR | O_CREAT | O_NONBLOCK, S_IRUSR | S_IWUSR, &mqa);

	if(mqi->mqd < 0) {
		ERR2(errno, "%s: mq_open failed", i->name);
		goto out_free_mq_name;
	}

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

	if(mq_close(mqi->mqd) != 0)
		ERR2(errno, "%s: mq_close failed on message queue %s", i->name, mqi->mq_name);

	ret = mq_unlink(mqi->mq_name);

	if( ret < 0 && errno !=ENOENT) {
		ERR2(errno, "%s: mq_unlink failed on queue %s", i->name, mqi->mq_name);
	}

	free(mqi->mq_name);
	free(mqi);
}

static int mqueue_read(ubx_block_t *i, ubx_data_t* data)
{
	int ret=0, size;
	struct mqueue_info* mqi;

	mqi = (struct mqueue_info*) i->private_data;
	size = data_size(data);
	ret = mq_receive(mqi->mqd, (char*) data->data, size, NULL);

	if(ret <= 0 && errno != EAGAIN) { /* error */
		ERR2(errno, "%s: mq_receive failed", i->name);
		mqi->cnt_recv_err++;
		goto out;
	} else if (ret<=0 && errno == EAGAIN) { /* empty queue */
		ret = 0;
		goto out;
	}

	ret /= data->type->size;
 out:
	return ret;
}

static void mqueue_write(ubx_block_t *i, ubx_data_t* data)
{
	int ret, size;
	struct mqueue_info* mqi;

	mqi = (struct mqueue_info*) i->private_data;

	if(mqi->type != data->type) {
		ERR("%s: invalid message type %s", i->name, data->type->name);
		goto out;
	}

	/* we let mq_send catch too large messages */
	size = data_size(data);

	ret = mq_send(mqi->mqd, (const char*) data->data, size, 1);

	if(ret != 0 ) {
		mqi->cnt_send_err++;
		if (errno != EAGAIN) {
			ERR2(errno, "%s: sending message failed", i->name);
			goto out;
		}
	}

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
	return ubx_block_register(ni, &mqueue_comp);
}

static void mqueue_mod_cleanup(ubx_node_info_t *ni)
{
	ubx_block_unregister(ni, "mqueue");
}

UBX_MODULE_INIT(mqueue_mod_init)
UBX_MODULE_CLEANUP(mqueue_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
