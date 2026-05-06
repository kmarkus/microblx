/*
 * Linux IIO buffered block using libiio
 *
 * Channels sample at hardware rate (sampling_frequency); the ptrig-driven
 * step non-blocking polls each device's buffer and emits only when new data
 * is available. Input channels only — use ubx/iio for DAC output.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#undef UBX_DEBUG

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <poll.h>
#include <iio.h>

#include "ubx.h"

#include "types/iio_config.h"
#include "types/iio_config.h.hexarr"

/* shared type with ubx/iio — tolerate duplicate registration */
static ubx_type_t iio_buf_channel_config_type =
	def_struct_type(struct ubx_iio_channel_config, &iio_config_h);
def_type_accessors(iio_channel_config, struct ubx_iio_channel_config)

char iio_buf_meta[] =
	"{ doc='Linux IIO buffered block: hardware-rate sampling via libiio buffers' }";

ubx_proto_config_t iio_buf_configs[] = {
	{
		.name = "channels",
		.type_name = "struct ubx_iio_channel_config",
		.min = 1,
		.max = 0,
		.doc = "array of IIO input channel configurations"
	},
	{
		.name = "trigger",
		.type_name = "char",
		.min = 0,
		.max = 1,
		.doc = "IIO trigger name (optional; empty = device's current trigger)"
	},
	{
		.name = "buffer_len",
		.type_name = "int",
		.min = 0,
		.max = 1,
		.doc = "kernel buffer depth in samples (default 4; must be power of 2 for many drivers)"
	},
	{
		.name = "timeout_ms",
		.type_name = "int",
		.min = 0,
		.max = 1,
		.doc = "per-step poll timeout: 0=non-blocking (default), -1=block until data"
	},
	{ .name = "loglevel", .type_name = "int" },
	{ 0 },
};

/* per-channel state */
struct iio_buf_entry {
	struct iio_channel *ch;
	double scale;
	double offset;
	ubx_port_t *port;
};

/* per-device group: channels sharing a buffer */
struct iio_dev_group {
	struct iio_device  *dev;
	struct iio_buffer  *buf;    /* created in start, destroyed in stop */
	int                 poll_fd;
	long               *entry_idx;
	long                num_entries;
};

struct iio_buf_info {
	struct iio_context *ctx;
	const struct ubx_iio_channel_config *cfgs;
	long num_channels;
	struct iio_buf_entry *entries;

	struct iio_dev_group *dev_groups;
	long num_groups;

	int timeout_ms;
};

/* Convert one buffer sample to double, handling all IIO data formats correctly. */
static double convert_sample(ubx_block_t *b, const struct iio_channel *ch, void *src)
{
	const struct iio_data_format *fmt = iio_channel_get_data_format(ch);

	if (fmt->is_signed) {
		switch (fmt->length / 8) {
		case 1: { int8_t  v; iio_channel_convert(ch, &v, src); return (double)v; }
		case 2: { int16_t v; iio_channel_convert(ch, &v, src); return (double)v; }
		case 4: { int32_t v; iio_channel_convert(ch, &v, src); return (double)v; }
		case 8: { int64_t v; iio_channel_convert(ch, &v, src); return (double)v; }
		}
	} else {
		switch (fmt->length / 8) {
		case 1: { uint8_t  v; iio_channel_convert(ch, &v, src); return (double)v; }
		case 2: { uint16_t v; iio_channel_convert(ch, &v, src); return (double)v; }
		case 4: { uint32_t v; iio_channel_convert(ch, &v, src); return (double)v; }
		case 8: { uint64_t v; iio_channel_convert(ch, &v, src); return (double)v; }
		}
	}
	ubx_err(b, "unsupported IIO data format length %u bits", fmt->length);
	return NAN;
}

/* Find a device group by device pointer; return index or -1. */
static long find_group(const struct iio_dev_group *groups, long n,
		       const struct iio_device *dev)
{
	for (long i = 0; i < n; i++)
		if (groups[i].dev == dev)
			return i;
	return -1;
}

/* Find device by name attr or by ID ("iio:deviceN"). */
static struct iio_device *iio_buf_find_dev(struct iio_context *ctx, const char *name)
{
	struct iio_device *dev = iio_context_find_device(ctx, name);
	if (dev)
		return dev;

	unsigned int n = iio_context_get_devices_count(ctx);
	for (unsigned int i = 0; i < n; i++) {
		dev = iio_context_get_device(ctx, i);
		if (!dev) continue;
		if (strcmp(iio_device_get_id(dev), name) == 0)
			return dev;
	}
	return NULL;
}

/* Destroy all buffers and disable all channels. Safe to call partial state. */
static void cleanup_groups(ubx_block_t *b)
{
	struct iio_buf_info *inf = b->private_data;

	for (long gi = 0; gi < inf->num_groups; gi++) {
		struct iio_dev_group *g = &inf->dev_groups[gi];
		if (g->buf) {
			iio_buffer_destroy(g->buf);
			g->buf = NULL;
			g->poll_fd = -1;
		}
		for (long j = 0; j < g->num_entries; j++)
			iio_channel_disable(inf->entries[g->entry_idx[j]].ch);
	}
}

static int iio_buf_init(ubx_block_t *b)
{
	long len;
	long i = 0;
	struct iio_buf_info *inf;

	b->private_data = calloc(1, sizeof(struct iio_buf_info));
	if (!b->private_data) {
		ubx_crit(b, "ENOMEM");
		return EOUTOFMEM;
	}
	inf = (struct iio_buf_info *)b->private_data;

	inf->ctx = iio_create_local_context();
	if (!inf->ctx) {
		ubx_err(b, "failed to create IIO local context");
		goto out_free_inf;
	}

	len = cfg_getptr_iio_channel_config(b, "channels", &inf->cfgs);
	if (len <= 0) {
		ubx_err(b, "EINVALID_CONFIG: 'channels' not set or invalid");
		goto out_destroy_ctx;
	}
	inf->num_channels = len;

	inf->entries = calloc(len, sizeof(struct iio_buf_entry));
	if (!inf->entries) {
		ubx_crit(b, "ENOMEM");
		goto out_destroy_ctx;
	}

	/* allocate dev_groups: worst case one group per channel */
	inf->dev_groups = calloc(len, sizeof(struct iio_dev_group));
	if (!inf->dev_groups) {
		ubx_crit(b, "ENOMEM");
		goto out_free_entries;
	}
	for (long j = 0; j < len; j++) {
		inf->dev_groups[j].entry_idx = calloc(len, sizeof(long));
		inf->dev_groups[j].poll_fd = -1;
		if (!inf->dev_groups[j].entry_idx) {
			ubx_crit(b, "ENOMEM");
			inf->num_groups = j; /* so cleanup frees allocated so far */
			goto out_cleanup;
		}
	}

	for (i = 0; i < inf->num_channels; i++) {
		const struct ubx_iio_channel_config *cfg = &inf->cfgs[i];
		struct iio_buf_entry *e = &inf->entries[i];

		if (cfg->direction) {
			ubx_err(b, "EINVALID_CONFIG: '%s/%s': ubx/iio_buf is input-only; use ubx/iio for output channels",
				cfg->device, cfg->channel);
			goto out_cleanup;
		}
		if (cfg->device[0] == '\0') {
			ubx_err(b, "EINVALID_CONFIG: channel[%ld]: empty device", i);
			goto out_cleanup;
		}
		if (cfg->channel[0] == '\0') {
			ubx_err(b, "EINVALID_CONFIG: channel[%ld]: empty channel name", i);
			goto out_cleanup;
		}

		struct iio_device *dev = iio_buf_find_dev(inf->ctx, cfg->device);
		if (!dev) {
			ubx_err(b, "EINVALID_CONFIG: device '%s' not found", cfg->device);
			goto out_cleanup;
		}

		e->ch = iio_device_find_channel(dev, cfg->channel, false);
		if (!e->ch) {
			ubx_err(b, "EINVALID_CONFIG: '%s/%s': input channel not found",
				cfg->device, cfg->channel);
			goto out_cleanup;
		}

		if (cfg->scale != 0.0) {
			e->scale  = cfg->scale;
			e->offset = cfg->offset;
		} else {
			if (iio_channel_attr_read_double(e->ch, "scale", &e->scale) < 0)
				e->scale = 1.0;
			if (iio_channel_attr_read_double(e->ch, "offset", &e->offset) < 0)
				e->offset = 0.0;
		}

		/* assign to device group (find or create) */
		long gi = find_group(inf->dev_groups, inf->num_groups, dev);
		if (gi < 0) {
			gi = inf->num_groups++;
			inf->dev_groups[gi].dev = dev;
		}
		inf->dev_groups[gi].entry_idx[inf->dev_groups[gi].num_entries++] = i;

		if (ubx_outport_add(b, cfg->channel, "IIO buffered input channel (physical value)",
				    0, "double", 1) < 0) {
			ubx_err(b, "'%s/%s': failed to add port", cfg->device, cfg->channel);
			goto out_cleanup;
		}
		e->port = ubx_port_get(b, cfg->channel);
		if (!e->port) {
			ubx_err(b, "'%s/%s': port_get failed", cfg->device, cfg->channel);
			i++;
			goto out_cleanup;
		}
	}
	return 0;

out_cleanup:
	for (long j = 0; j < i; j++)
		ubx_port_rm(b, inf->cfgs[j].channel);
	for (long j = 0; j < len; j++)
		free(inf->dev_groups[j].entry_idx);
	free(inf->dev_groups);
out_free_entries:
	free(inf->entries);
out_destroy_ctx:
	iio_context_destroy(inf->ctx);
out_free_inf:
	free(b->private_data);
	b->private_data = NULL;
	return -1;
}

static int iio_buf_start(ubx_block_t *b)
{
	struct iio_buf_info *inf = b->private_data;

	const int *p_int;
	long ilen;

	ilen = cfg_getptr_int(b, "buffer_len", &p_int);
	int buffer_len = (ilen > 0 && *p_int > 0) ? *p_int : 4;

	ilen = cfg_getptr_int(b, "timeout_ms", &p_int);
	inf->timeout_ms = (ilen > 0) ? *p_int : 0;

	const char *trig_name = NULL;
	long trig_len = cfg_getptr_char(b, "trigger", &trig_name);

	for (long gi = 0; gi < inf->num_groups; gi++) {
		struct iio_dev_group *g = &inf->dev_groups[gi];
		const char *devname = iio_device_get_name(g->dev);

		/* enable scan elements for all channels in this group */
		for (long j = 0; j < g->num_entries; j++)
			iio_channel_enable(inf->entries[g->entry_idx[j]].ch);

		/* validate and apply sampling_frequency (must be consistent per device) */
		double sf = 0.0;
		for (long j = 0; j < g->num_entries; j++) {
			double csf = inf->cfgs[g->entry_idx[j]].sampling_frequency;
			if (csf <= 0.0)
				continue;
			if (sf > 0.0 && sf != csf) {
				ubx_err(b, "EINVALID_CONFIG: device '%s': conflicting sampling_frequency values (%.1f vs %.1f); all channels on the same device must agree",
					devname, sf, csf);
				goto out_cleanup;
			}
			sf = csf;
		}
		if (sf > 0.0) {
			/* try channel attr first (rare), then device attr (common) */
			int sret = iio_channel_attr_write_double(
				inf->entries[g->entry_idx[0]].ch,
				"sampling_frequency", sf);
			if (sret < 0)
				sret = iio_device_attr_write_double(g->dev,
								    "sampling_frequency", sf);
			if (sret < 0) {
				ubx_err(b, "EINVALID_CONFIG: device '%s': failed to set sampling_frequency %.1f Hz (%d)",
					devname, sf, sret);
				goto out_cleanup;
			}
		}

		/* optionally assign trigger (same trigger applied to all device groups) */
		if (trig_len > 0 && trig_name && trig_name[0] != '\0') {
			struct iio_device *trig =
				iio_context_find_device(inf->ctx, trig_name);
			if (!trig) {
				ubx_err(b, "EINVALID_CONFIG: trigger '%s' not found", trig_name);
				goto out_cleanup;
			}
			if (iio_device_set_trigger(g->dev, trig) < 0) {
				ubx_err(b, "device '%s': failed to set trigger '%s'",
					devname, trig_name);
				goto out_cleanup;
			}
		}

		g->buf = iio_device_create_buffer(g->dev, (size_t)buffer_len, false);
		if (!g->buf) {
			ubx_err(b, "device '%s': failed to create buffer (len=%d): %m",
				devname, buffer_len);
			goto out_cleanup;
		}

		g->poll_fd = iio_buffer_get_poll_fd(g->buf);
	}
	return 0;

out_cleanup:
	cleanup_groups(b);
	return -1;
}

static void iio_buf_stop(ubx_block_t *b)
{
	cleanup_groups(b);
}

static void iio_buf_cleanup(ubx_block_t *b)
{
	struct iio_buf_info *inf = b->private_data;
	if (!inf)
		return;

	for (long i = 0; i < inf->num_channels; i++)
		ubx_port_rm(b, inf->cfgs[i].channel);

	for (long i = 0; i < inf->num_channels; i++) /* num_channels = max allocated groups */
		free(inf->dev_groups[i].entry_idx);
	free(inf->dev_groups);
	free(inf->entries);
	iio_context_destroy(inf->ctx);
	free(b->private_data);
	b->private_data = NULL;
}

static void iio_buf_step(ubx_block_t *b)
{
	struct iio_buf_info *inf = b->private_data;

	for (long gi = 0; gi < inf->num_groups; gi++) {
		struct iio_dev_group *g = &inf->dev_groups[gi];

		struct pollfd pfd = { .fd = g->poll_fd, .events = POLLIN };
		if (poll(&pfd, 1, inf->timeout_ms) <= 0)
			continue; /* no data yet or error — ports keep last value */

		ssize_t nread = iio_buffer_refill(g->buf);
		if (nread < 0) {
			ubx_err(b, "group[%ld]: refill failed (%zd)", gi, nread);
			continue;
		}

		for (long j = 0; j < g->num_entries; j++) {
			struct iio_buf_entry *e = &inf->entries[g->entry_idx[j]];

			/* take the most recent (last) sample from the buffer */
			void *last = NULL;
			for (void *p = iio_buffer_first(g->buf, e->ch);
			     p < iio_buffer_end(g->buf);
			     p += iio_buffer_step(g->buf))
				last = p;

			if (!last)
				continue;

			double raw = convert_sample(b, e->ch, last);
			double val = (raw + e->offset) * e->scale;
			write_double(e->port, &val);
		}
	}
}

ubx_proto_block_t iio_buf_comp = {
	.name     = "ubx/iio_buf",
	.meta_data = iio_buf_meta,
	.type     = BLOCK_TYPE_COMPUTATION,
	.configs  = iio_buf_configs,
	.init     = iio_buf_init,
	.start    = iio_buf_start,
	.stop     = iio_buf_stop,
	.cleanup  = iio_buf_cleanup,
	.step     = iio_buf_step,
};

int iio_buf_module_init(ubx_node_t *nd)
{
	int ret = ubx_type_register(nd, &iio_buf_channel_config_type);
	if (ret != 0 && ret != EALREADY_REGISTERED)
		return -1;
	return ubx_block_register(nd, &iio_buf_comp);
}

void iio_buf_module_cleanup(ubx_node_t *nd)
{
	ubx_type_unregister(nd, iio_buf_channel_config_type.name);
	ubx_block_unregister(nd, "ubx/iio_buf");
}

UBX_MODULE_INIT(iio_buf_module_init)
UBX_MODULE_CLEANUP(iio_buf_module_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
