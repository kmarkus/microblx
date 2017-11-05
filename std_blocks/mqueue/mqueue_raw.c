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
	{ .name="mq_maxmsg", .type_name = "long" },
	{ .name="mq_msgsize", .type_name = "long" },
	{ NULL }
};

struct mqueue_info {
	mqd_t mqd;
	char *mq_name;
	struct mqueue_msg *msg;
	ubx_type_t* type;

	unsigned long cnt_send_err;
	unsigned long cnt_recv_err;
};


static int mqueue_init(ubx_block_t *i)
{
	int ret=-1;
	unsigned int tmplen;
	char* chrptr;

	struct mqueue_info* mqi;
	struct mq_attr mqa;

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

	if(chrptr[0] != '/') {
		ERR("%s: invalid mq_name, must start with '/' (cfr. mq_overview(1))", i->name);
		goto out_free_info;
	}

	mqi->mq_name = strdup(chrptr);

	/* get mq_attr configs */
	mqa.mq_maxmsg = *((uint32_t*) ubx_config_get_data_ptr(i, "mq_maxmsg", &tmplen));

	if (mqa.mq_maxmsg <= 0) {
		ERR("%s: invalid value for mq_maxmsg: %ld", i->name, mqa.mq_maxmsg);
		goto out_free_mq_name;
	}

	mqa.mq_msgsize = *((uint32_t*) ubx_config_get_data_ptr(i, "mq_msgsize", &tmplen));

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
	int ret, size;
	struct mqueue_info* mqi;

	mqi = (struct mqueue_info*) i->private_data;
	size = data_size(data);

	ret = mq_receive(mqi->mqd, (char*) data->data, size, NULL);

	if(ret <= 0 && errno != EAGAIN) {
		ERR2(errno, "%s: mq_receive failed", i->name);
		mqi->cnt_recv_err++;
	}

	return 0;
}

static void mqueue_write(ubx_block_t *i, ubx_data_t* data)
{
	int ret, size;
	struct mqueue_info* mqi;

	mqi = (struct mqueue_info*) i->private_data;
	size = data_size(data);

	ret = mq_send(mqi->mqd, (const char*) data->data, size, 1);

	if(ret != 0 ) {
		mqi->cnt_send_err++;
		if (ret != EAGAIN) {
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
