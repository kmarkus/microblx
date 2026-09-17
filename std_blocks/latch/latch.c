/*
 * latch: a single-writer, multi-reader latest-value iblock
 */

#undef UBX_DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include "ubx.h"

/*
 * Bound for a reader retrying against a writer that keeps landing inside
 * its copy window. Exhausting this returns "no data", which every caller
 * already handles, so a low priority reader degrades instead of hanging.
 */
#define LATCH_SPIN_RETRIES	1000

char latch_meta[] =
	"{ doc='single-writer multi-reader latest-value store',"
	"  description=[[ A latch holds the most recently written value and"
	"                 returns it to every read. Reads do not consume, so"
	"                 readers do not interact and any number of them may"
	"                 share one latch -- unlike lfrb, where a second reader"
	"                 steals samples from the first."
	""
	"                 This is state, not a queue. The return value means"
	"                 'a value exists', not 'a new value arrived', and a"
	"                 reader cannot tell a repeat from a fresh sample."
	"                 That is the point, and it is also the hazard: a"
	"                 control block that must act only on new input, or an"
	"                 aggregator that records which samples were absent,"
	"                 needs lfrb or vstore instead. Use a latch for"
	"                 published state and for write-on-change diagnostics"
	"                 that are sampled by an unrelated cycle."
	""
	"                 Concurrency is a seqlock: the writer is wait-free"
	"                 (two plain stores and two fences, no read-modify-"
	"                 write), and a reader that catches a write in progress"
	"                 retries. Cross-thread use is safe, which vstore's is"
	"                 not. Exactly one writer is required -- two concurrent"
	"                 writers interleave the sequence counter and can"
	"                 publish a torn value that passes the reader's check."
	""
	"                 Before the first write a latch reads as no-data, so"
	"                 the reader never sees the zeroed buffer as if it were"
	"                 a value. ]],"
	"  version=0.1,"
	"  hard_real_time=true,"
	"}";

ubx_proto_config_t latch_config[] = {
	{ .name="type_name", .type_name="char",     .min=1,         .doc="name of registered microblx type to transport" },
	{ .name="data_len",  .type_name="uint32_t", .min=0, .max=1, .doc="array length (multiplier) of data (default: 1)" },
	{ 0 },
};

/*
 * No diagnostic port, deliberately. lfrb's overruns and vstore's overwrites
 * are both produced by the single writer. The analogous latch statistic -- a
 * torn read -- is produced by readers, and writing one ubx port from several
 * reader threads is exactly the unsynchronised sharing this block exists to
 * avoid.
 */

struct latch_info {
	const ubx_type_t *type;
	long data_len;			/* array length */

	atomic_ulong seq;		/* even: stable, odd: write in progress */
	atomic_long len;		/* elements written, 0 = never written */

	uint8_t data[];			/* tail-allocated, see latch_init */
};

int latch_init(ubx_block_t *i)
{
	long len;
	const uint32_t *val;
	const char *type_name;
	const ubx_type_t *type;
	long data_len;
	struct latch_info *inf;

	len = cfg_getptr_uint32(i, "data_len", &val);
	if (len < 0)
		return len;

	data_len = (len > 0) ? *val : 1;

	if (data_len <= 0) {
		ubx_err(i, "EINVALID_CONFIG: data_len=%ld", data_len);
		return EINVALID_CONFIG;
	}

	len = cfg_getptr_char(i, "type_name", &type_name);
	assert(len > 0);

	type = ubx_type_get(i->nd, type_name);

	if (type == NULL) {
		ubx_err(i, "EINVALID_CONFIG: unknown type %s", type_name);
		return EINVALID_CONFIG;
	}

	/* One allocation for the counters and the payload they guard, so the
	 * common case costs one cache line, as in vstore. */
	i->private_data = calloc(1, sizeof(struct latch_info) + data_len * type->size);

	if (i->private_data == NULL) {
		ubx_err(i, "EOUTOFMEM: failed to alloc latch_info");
		return EOUTOFMEM;
	}

	inf = (struct latch_info *)i->private_data;
	inf->type = type;
	inf->data_len = data_len;

	/* calloc gives seq=0 (even, stable) and len=0 (no value yet) */

	ubx_debug(i, "%s: %s [%ld], %zu bytes total", __func__, type_name,
		  data_len, sizeof(struct latch_info) + data_len * type->size);

	return 0;
}

void latch_cleanup(ubx_block_t *i)
{
	free(i->private_data);
	i->private_data = NULL;
}

void latch_write(ubx_block_t *i, const ubx_data_t *msg)
{
	struct latch_info *inf = (struct latch_info *)i->private_data;
	unsigned long seq;
	long writelen;

	if (inf->type != msg->type) {
		ubx_err(i, "%s: invalid message type %s", __func__, msg->type->name);
		return;
	}

	if (msg->len > inf->data_len) {
		ubx_err(i, "%s: only copying %ld of %lu array elements",
			__func__, inf->data_len, msg->len);
	}

	/* Record what was actually written, not what the buffer can hold: a
	 * writer may send fewer array elements than data_len, and the reader
	 * must not be handed the stale tail as if it were data. */
	writelen = MIN((long)msg->len, inf->data_len);

	/* Odd sequence: a concurrent reader sees a write in progress and
	 * retries rather than copying. Relaxed is enough for the counter
	 * itself; the fence is what orders it against the payload. */
	seq = atomic_load_explicit(&inf->seq, memory_order_relaxed);
	atomic_store_explicit(&inf->seq, seq + 1, memory_order_relaxed);
	atomic_thread_fence(memory_order_release);

	memcpy(inf->data, msg->data, inf->type->size * writelen);
	atomic_store_explicit(&inf->len, writelen, memory_order_relaxed);

	atomic_thread_fence(memory_order_release);
	atomic_store_explicit(&inf->seq, seq + 2, memory_order_relaxed);
}

long latch_read(ubx_block_t *i, ubx_data_t *msg)
{
	struct latch_info *inf = (struct latch_info *)i->private_data;
	unsigned long seq;
	long len, readlen;

	if (inf->type != msg->type) {
		ubx_err(i, "%s: invalid message type %s", __func__, msg->type->name);
		return EINVALID_TYPE;
	}

	for (int retries = LATCH_SPIN_RETRIES; retries > 0; retries--) {
		seq = atomic_load_explicit(&inf->seq, memory_order_relaxed);

		if (seq & 1)
			continue;	/* writer mid-copy */

		atomic_thread_fence(memory_order_acquire);

		/* len is only ever written as MIN(msg->len, data_len), so even
		 * a value from a previous write bounds the copy correctly. */
		len = atomic_load_explicit(&inf->len, memory_order_relaxed);

		if ((long)msg->len < len) {
			ubx_err(i, "%s: only copying %lu array elements of %ld",
				__func__, msg->len, len);
		}

		readlen = MIN((long)msg->len, len);

		/* May copy a half-written payload. That is the seqlock bargain:
		 * the sequence re-check below detects it and the bytes are
		 * discarded before the caller ever sees them. */
		memcpy(msg->data, inf->data, inf->type->size * readlen);

		atomic_thread_fence(memory_order_acquire);

		if (atomic_load_explicit(&inf->seq, memory_order_relaxed) == seq)
			return readlen;	/* 0 until the first write */
	}

	/* Starved out by a writer that keeps landing in the copy window. Same
	 * contract as an empty queue. */
	return 0;
}

ubx_proto_block_t latch_comp = {
	.name = "ubx/latch",
	.type = BLOCK_TYPE_INTERACTION,
	.meta_data = latch_meta,
	.configs = latch_config,

	.init = latch_init,
	.cleanup = latch_cleanup,

	/* iops */
	.write = latch_write,
	.read = latch_read,
};

int latch_mod_init(ubx_node_t *nd)
{
	return ubx_block_register(nd, &latch_comp);
}

void latch_mod_cleanup(ubx_node_t *nd)
{
	ubx_block_unregister(nd, "ubx/latch");
}

UBX_MODULE_INIT(latch_mod_init)
UBX_MODULE_CLEANUP(latch_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
