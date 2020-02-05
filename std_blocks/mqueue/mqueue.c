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
	"  realtime=true,"
	"}";

ubx_config_t mqueue_config[] = {
	{ .name="mq_name", .type_name = "char", .min=2, .max=NAME_MAX, .doc="mqueue name see mq_overview(7)" },
	{ .name="type_name", .type_name = "char", .min=1, .doc="name of registered microblx type to transport" },
	{ .name="data_len", .type_name = "uint32_t", .max=1, .doc="array length (multiplier) of data (default: 1)" },
	{ .name="buffer_len", .type_name = "uint32_t", .min=1, .max=1, .doc="max number of data elements the buffer shall hold" },
	{ NULL }
};

struct mqueue_info {
	mqd_t mqd;
	char *mq_name;
	struct mqueue_msg *msg;

	const ubx_type_t* type;		/* type of contained elements */
	uint32_t data_len;		/* buffer size of each element */
	uint32_t buffer_len;		/* number of elements */

	unsigned long cnt_send_err;
	unsigned long cnt_recv_err;
};


static int mqueue_init(ubx_block_t *i)
{
	int ret=-1;
	long len;
	const uint32_t *val;
	const char* chrptr;

	struct mqueue_info* mqi;
	struct mq_attr mqa;

	if((i->private_data = calloc(1, sizeof(struct mqueue_info)))==NULL) {
		ubx_err(i, "failed to alloc mqueue_info");
		ret = EOUTOFMEM;
		goto out;
	}

	mqi = (struct mqueue_info*) i->private_data;

	/* config buffer_len */
	if ((len = cfg_getptr_uint32(i, "buffer_len", &val)) < 0) {
		ubx_err(i, "%s: failed to get buffer_len config", i->name);
		goto out_free_info;
	}

	if (*val==0) {
		ubx_err(i, "%s: illegal value buffer_len=0", i->name);
		ret = EINVALID_CONFIG;
		goto out_free_info;
	}

	mqi->buffer_len =*val;

	mqa.mq_maxmsg = mqi->buffer_len;

	/* config data_len */
	if ((len = cfg_getptr_uint32(i, "data_len", &val)) < 0) {
		ubx_err(i, "%s: failed to read 'data_len' config", i->name);
		goto out_free_info;
	}

	mqi->data_len = (len>0) ? *val : 1;

	/* config type_name */
	if ((len = cfg_getptr_char(i, "type_name", &chrptr)) < 0) {
		ubx_err(i, "failed to access config %s", i->name);
		goto out_free_info;
	}

	mqi->type=ubx_type_get(i->ni, chrptr);

	if (mqi->type == NULL) {
		ubx_err(i, "%s: failed to lookup type %s", i->name, chrptr);
		ret = EINVALID_CONFIG;
		goto out_free_info;
	}

	/* configure max message size */
	mqa.mq_msgsize = mqi->data_len * mqi->type->size;

	/* retrive mq_name config */
	if ((len = cfg_getptr_char(i, "mq_name", &chrptr)) < 0) {
		ubx_err(i, "failed to access config %s", i->name);
		goto out_free_info;
	}

	if(chrptr[0] != '/') {
		ubx_err(i, "missing '/' in mq_name %s (cfr. mq_overview(1))", i->name);
		goto out_free_info;
	}

	mqi->mq_name = strdup(chrptr);

	if (mqa.mq_msgsize <= 0) {
		ubx_err(i, "%s: invalid value for mq_msgsize: %ld", i->name, mqa.mq_msgsize);
		goto out_free_mq_name;
	}

	mqa.mq_flags = O_NONBLOCK;

	mqi->mqd = mq_open(mqi->mq_name, O_RDWR | O_CREAT | O_NONBLOCK, S_IRUSR | S_IWUSR, &mqa);

	if(mqi->mqd < 0) {
		ubx_err(i, "mq_open failed %s", strerror(errno));
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

	if(mq_close(mqi->mqd) != 0) {
		ubx_err(i, "mq_close %s failed: %s", mqi->mq_name, strerror(errno));
	}

	ret = mq_unlink(mqi->mq_name);

	if( ret < 0 && errno !=ENOENT) {
		ubx_err(i, "mq_unlink %s failed: %s", mqi->mq_name, strerror(errno));
	}

	free(mqi->mq_name);
	free(mqi);
}

static long mqueue_read(ubx_block_t *i, ubx_data_t* data)
{
	int ret=0, size;
	struct mqueue_info* mqi;

	mqi = (struct mqueue_info*) i->private_data;
	size = data_size(data);
	ret = mq_receive(mqi->mqd, (char*) data->data, size, NULL);

	if(ret <= 0 && errno != EAGAIN) { /* error */
		ubx_err(i, "mq_receive %s failed: %s", i->name, strerror(errno));
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
		ubx_err(i, "%s: invalid message type %s", i->name, data->type->name);
		goto out;
	}

	/* we let mq_send catch too large messages */
	size = data_size(data);

	ret = mq_send(mqi->mqd, (const char*) data->data, size, 1);

	if(ret != 0 ) {
		mqi->cnt_send_err++;
		if (errno != EAGAIN) {
			ubx_err(i, "mq_send %s failed: %s", i->name, strerror(errno));
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
