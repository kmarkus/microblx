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
	{ .name = "mq_id", .type_name = "char", .min = 1, .max = NAME_MAX, .doc = "mqueue base id" },
	{ .name = "type_name", .type_name = "char", .min = 1, .doc = "name of registered microblx type to transport" },
	{ .name = "data_len", .type_name = "long", .max = 1, .doc = "array length (multiplier) of data (default: 1)" },
	{ .name = "buffer_len", .type_name = "long", .min = 1, .max = 1, .doc = "max number of data elements the buffer shall hold" },
	{ .name = "blocking", .type_name = "uint32_t", .min = 0, .max = 1, .doc = "enable blocking mode (def: 0)" },
	{ 0 }
};

struct mqueue_info {
	mqd_t mqd;
	const char *mq_id;
	char mq_name[NAME_MAX+1];

	struct mqueue_msg *msg;

	const ubx_type_t *type;		/* type of contained elements */
	long data_len;			/* buffer size of each element */
	long buffer_len;		/* number of elements */

	unsigned long cnt_send_err;
	unsigned long cnt_recv_err;
};


static int mqueue_init(ubx_block_t *i)
{
	int ret = -1;
	long len, mq_flags = 0;
	const uint32_t *val;
	const long *val_long;
	const char *chrptr;

	char hexhash[TYPE_HASHSTR_LEN+1];

	struct mqueue_info *inf;
	struct mq_attr mqa;

	i->private_data = calloc(1, sizeof(struct mqueue_info));

	if (i->private_data == NULL) {
		ubx_err(i, "failed to alloc mqueue_info");
		ret = EOUTOFMEM;
		goto out;
	}

	inf = (struct mqueue_info *)i->private_data;

	/* retrive mq_id config */
	len = cfg_getptr_char(i, "mq_id", &inf->mq_id);
	if (len < 0) {
		ubx_err(i, "failed to access config mq_id");
		goto out_free_info;
	}

	/* config buffer_len */
	len = cfg_getptr_long(i, "buffer_len", &val_long);
	if (len < 0) {
		ubx_err(i, "failed to get buffer_len config");
		goto out_free_info;
	}

	if (*val_long == 0) {
		ubx_err(i, "EINVALID_CONFIG: illegal value buffer_len=0");
		ret = EINVALID_CONFIG;
		goto out_free_info;
	}

	inf->buffer_len = *val_long;

	mqa.mq_maxmsg = inf->buffer_len;

	/* config data_len */
	len = cfg_getptr_long(i, "data_len", &val_long);
	if (len < 0) {
		ubx_err(i, "EINVALID_CONFIG: failed to read 'data_len' config");
		goto out_free_info;
	}

	inf->data_len = (len > 0) ? *val_long : 1;

	/* config type_name */
	len = cfg_getptr_char(i, "type_name", &chrptr);
	if (len < 0) {
		ubx_err(i, "failed to access config 'type_name'");
		goto out_free_info;
	}

	inf->type = ubx_type_get(i->ni, chrptr);

	if (inf->type == NULL) {
		ubx_err(i, "failed to lookup type %s", chrptr);
		ret = EINVALID_CONFIG;
		goto out_free_info;
	}

	/* configure max message size */
	mqa.mq_msgsize = inf->data_len * inf->type->size;

	if (mqa.mq_msgsize <= 0) {
		ubx_err(i, "invalid value for mq_msgsize: %ld", mqa.mq_msgsize);
		goto out_free_info;
	}

	/* construct mq_name */
	ubx_type_hashstr(inf->type, hexhash);

	ret = snprintf(inf->mq_name, NAME_MAX+1, "/ubx_%s_%li_%s",
		       hexhash, inf->data_len, inf->mq_id);

	if (ret < 0) {
		ubx_err(i, "failed to construct mqueue name");
		goto out_free_info;
	}

	/* blocking mode */
	len = cfg_getptr_uint32(i, "blocking", &val);
	if (len < 0) {
		ubx_err(i, "failed to get blocking config");
		goto out_free_info;
	}

	if (len == 0 || (len > 0 && *val == 0))
		mq_flags = O_NONBLOCK;

	inf->mqd = mq_open(inf->mq_name, O_RDWR | O_CREAT | mq_flags, 0600, &mqa);

	if (inf->mqd < 0) {
		ubx_err(i, "mq_open for %s failed: %s", inf->mq_name, strerror(errno));
		goto out_free_info;
	}

	ubx_info(i, "created %s %s[%lu] mq %s with %lu elem",
		 (mq_flags & O_NONBLOCK) ? "non-blocking" : "blocking",
		 inf->type->name, inf->data_len, inf->mq_id, inf->buffer_len);

	ret = 0;
	goto out;

 out_free_info:
	free(inf);
 out:
	return ret;
}

static void mqueue_cleanup(ubx_block_t *i)
{
	int ret;
	struct mqueue_info *inf = (struct mqueue_info *)i->private_data;

	if (mq_close(inf->mqd) != 0)
		ubx_err(i, "mq_close %s failed: %s", inf->mq_name, strerror(errno));

	ret = mq_unlink(inf->mq_name);

	if (ret < 0 && errno != ENOENT)
		ubx_err(i, "mq_unlink %s failed: %s", inf->mq_name, strerror(errno));

	free(inf);
}

static long mqueue_read(ubx_block_t *i, ubx_data_t *data)
{
	int ret = 0, size;
	struct mqueue_info *inf;

	if (i->block_state != BLOCK_STATE_ACTIVE) {
		ubx_err(i, "EWRONG_STATE: mqueue_read in state %s",
			block_state_tostr(i->block_state));
		return -1;
	}

	inf = (struct mqueue_info *)i->private_data;
	size = data_size(data);
	ret = mq_receive(inf->mqd, (char *)data->data, size, NULL);

	if (ret <= 0 && errno != EAGAIN) { /* error */
		ubx_err(i, "mq_receive %s failed: %s", i->name, strerror(errno));
		inf->cnt_recv_err++;
		goto out;
	} else if (ret <= 0 && errno == EAGAIN) { /* empty queue */
		ret = 0;
		goto out;
	}

	ret /= data->type->size;
 out:
	return ret;
}

static void mqueue_write(ubx_block_t *i, ubx_data_t *data)
{
	int ret, size;
	struct mqueue_info *inf;

	if (i->block_state != BLOCK_STATE_ACTIVE) {
		ubx_err(i, "EWRONG_STATE: mqueue_write in state %s",
			block_state_tostr(i->block_state));
		return;
	}

	inf = (struct mqueue_info *)i->private_data;

	if (inf->type != data->type) {
		ubx_err(i, "invalid message type %s", data->type->name);
		goto out;
	}

	/* we let mq_send catch too large messages */
	size = data_size(data);

	ret = mq_send(inf->mqd, (const char *)data->data, size, 1);

	if (ret != 0) {
		inf->cnt_send_err++;
		if (errno != EAGAIN) {
			ubx_err(i, "mq_send failed: %s", strerror(errno));
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
	.read = mqueue_read,
	.write = mqueue_write,

};

static int mqueue_mod_init(ubx_node_info_t *ni)
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
