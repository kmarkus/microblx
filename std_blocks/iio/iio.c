/*
 * Linux IIO block using libiio
 * SPDX-License-Identifier: BSD-3-Clause
 */

#undef UBX_DEBUG

#include <stdlib.h>
#include <string.h>
#include <iio.h>

#include "ubx.h"

#include "types/iio_config.h"
#include "types/iio_config.h.hexarr"

ubx_type_t iio_channel_config_type = def_struct_type(struct ubx_iio_channel_config, &iio_config_h);
def_type_accessors(iio_channel_config, struct ubx_iio_channel_config)

char iio_meta[] = "{ doc='Linux IIO block: reads/writes IIO channels via libiio' }";

ubx_proto_config_t iio_configs[] = {
	{
		.name = "channels",
		.type_name = "struct ubx_iio_channel_config",
		.min = 1,
		.max = 0,
		.doc = "array of IIO channel configurations"
	},
	{ .name = "loglevel", .type_name = "int" },
	{ 0 },
};

struct iio_entry {
	struct iio_channel *ch;
	int is_output;		/* 1: actuator/DAC → ubx in-port; 0: sensor/ADC → ubx out-port */
	double scale;		/* physical = (val + offset) * scale */
	double offset;
	const char *read_attr;	/* "raw" for IIO, "input" for hwmon channels */
	ubx_port_t *port;
};

struct iio_info {
	struct iio_context *ctx;
	const struct ubx_iio_channel_config *cfgs;
	long num_channels;
	struct iio_entry *entries;
};

/* Find device by name attr or by ID ("iio:deviceN") */
static struct iio_device *iio_find_dev(struct iio_context *ctx, const char *name)
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

static int iio_init(ubx_block_t *b)
{
	long len;
	long i = 0;
	struct iio_info *inf;

	b->private_data = calloc(1, sizeof(struct iio_info));
	if (!b->private_data) {
		ubx_crit(b, "ENOMEM");
		return EOUTOFMEM;
	}
	inf = (struct iio_info *)b->private_data;

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

	inf->entries = calloc(len, sizeof(struct iio_entry));
	if (!inf->entries) {
		ubx_crit(b, "ENOMEM");
		goto out_destroy_ctx;
	}

	for (i = 0; i < inf->num_channels; i++) {
		const struct ubx_iio_channel_config *cfg = &inf->cfgs[i];
		struct iio_entry *e = &inf->entries[i];
		int ret;

		if (cfg->device[0] == '\0') {
			ubx_err(b, "EINVALID_CONFIG: channel[%ld]: empty device", i);
			goto out_cleanup;
		}
		if (cfg->channel[0] == '\0') {
			ubx_err(b, "EINVALID_CONFIG: channel[%ld]: empty channel name", i);
			goto out_cleanup;
		}

		struct iio_device *dev = iio_find_dev(inf->ctx, cfg->device);
		if (!dev) {
			ubx_err(b, "EINVALID_CONFIG: device '%s' not found", cfg->device);
			goto out_cleanup;
		}

		e->ch = iio_device_find_channel(dev, cfg->channel, cfg->direction != 0);
		if (!e->ch) {
			ubx_err(b, "EINVALID_CONFIG: '%s/%s' (%s): channel not found",
				cfg->device, cfg->channel,
				cfg->direction ? "output" : "input");
			goto out_cleanup;
		}

		e->is_output = iio_channel_is_output(e->ch) ? 1 : 0;

		if (cfg->scale != 0.0) {
			e->scale  = cfg->scale;
			e->offset = cfg->offset;
		} else {
			if (iio_channel_attr_read_double(e->ch, "scale", &e->scale) < 0)
				e->scale = 1.0;
			if (iio_channel_attr_read_double(e->ch, "offset", &e->offset) < 0)
				e->offset = 0.0;
		}

		/* probe which attr is readable: IIO uses "raw", hwmon uses "input" */
		{
			long long probe;
			e->read_attr = (iio_channel_attr_read_longlong(e->ch, "raw", &probe) >= 0)
				? "raw" : "input";
		}

		if (cfg->sampling_frequency > 0.0) {
			int sret = iio_channel_attr_write_double(e->ch, "sampling_frequency",
								 cfg->sampling_frequency);
			if (sret < 0)
				sret = iio_device_attr_write_double(dev, "sampling_frequency",
								    cfg->sampling_frequency);
			if (sret < 0) {
				ubx_err(b, "EINVALID_CONFIG: '%s/%s': failed to set sampling_frequency %.1f Hz (%d)",
					cfg->device, cfg->channel, cfg->sampling_frequency, sret);
				goto out_cleanup;
			}
		}

		if (e->is_output)
			ret = ubx_inport_add(b, cfg->channel, "IIO output channel (physical value)", 0, "double", 1);
		else
			ret = ubx_outport_add(b, cfg->channel, "IIO input channel (physical value)", 0, "double", 1);

		if (ret < 0) {
			ubx_err(b, "'%s/%s': failed to add port", cfg->device, cfg->channel);
			e->ch = NULL;
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
	free(inf->entries);
out_destroy_ctx:
	iio_context_destroy(inf->ctx);
out_free_inf:
	free(b->private_data);
	b->private_data = NULL;
	return -1;
}

static void iio_cleanup(ubx_block_t *b)
{
	struct iio_info *inf = (struct iio_info *)b->private_data;
	if (!inf)
		return;

	for (long i = 0; i < inf->num_channels; i++)
		ubx_port_rm(b, inf->cfgs[i].channel);

	free(inf->entries);
	iio_context_destroy(inf->ctx);
	free(b->private_data);
}

static void iio_step(ubx_block_t *b)
{
	struct iio_info *inf = (struct iio_info *)b->private_data;

	for (long i = 0; i < inf->num_channels; i++) {
		struct iio_entry *e = &inf->entries[i];

		if (!e->is_output) {
			/* sensor/ADC: read value → compute physical → emit on out-port */
			long long raw;
			int ret = iio_channel_attr_read_longlong(e->ch, e->read_attr, &raw);
			if (ret < 0) {
				ubx_err(b, "channel[%ld]: read '%s' failed (%d)", i, e->read_attr, ret);
				continue;
			}
			double val = ((double)raw + e->offset) * e->scale;
			write_double(e->port, &val);
		} else {
			/* actuator/DAC: read physical from in-port → convert → write raw */
			double val = 0.0;
			if (read_double(e->port, &val) <= 0)
				continue;
			if (e->scale == 0.0) {
				ubx_err(b, "channel[%ld]: scale is zero, cannot convert to raw", i);
				continue;
			}
			long long raw = (long long)(val / e->scale - e->offset);
			int ret = iio_channel_attr_write_longlong(e->ch, "raw", raw);
			if (ret < 0)
				ubx_err(b, "channel[%ld]: write 'raw' failed (%d)", i, ret);
		}
	}
}

ubx_proto_block_t iio_comp = {
	.name = "ubx/iio",
	.meta_data = iio_meta,
	.type = BLOCK_TYPE_COMPUTATION,
	.configs = iio_configs,
	.init = iio_init,
	.cleanup = iio_cleanup,
	.step = iio_step,
};

int iio_module_init(ubx_node_t *nd)
{
	if (ubx_type_register(nd, &iio_channel_config_type))
		return -1;
	return ubx_block_register(nd, &iio_comp);
}

void iio_module_cleanup(ubx_node_t *nd)
{
	ubx_type_unregister(nd, iio_channel_config_type.name);
	ubx_block_unregister(nd, "ubx/iio");
}

UBX_MODULE_INIT(iio_module_init)
UBX_MODULE_CLEANUP(iio_module_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
