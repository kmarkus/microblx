/*
 * Generic Linux GPIO block based on libgpiod v2
 * SPDX-License-Identifier: BSD-3-Clause
 */

#undef UBX_DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <gpiod.h>

#include "ubx.h"
#include "trig_utils.h"

#include "types/gpio_config.h"
#include "types/gpio_config.h.hexarr"

ubx_type_t gpio_config_type = def_struct_type(struct ubx_gpio_config, &gpio_config_h);
def_type_accessors(gpio_config, struct ubx_gpio_config)

char gpio_meta[] = "{ doc='Generic Linux GPIO block based on libgpiod v2' }";

ubx_proto_config_t gpio_configs[] = {
	{
		.name = "gpios",
		.type_name = "struct ubx_gpio_config",
		.min = 1,
		.max = 0,
		.doc = "array of GPIO line configurations"
	},
	{
		.name = "trigee",
		.type_name = "struct ubx_triggee",
		.min = 0,
		.max = 1,
		.doc = "optional block to trigger between reading inputs and writing outputs"
	},
	{ .name = "loglevel", .type_name = "int" },
	{ 0 },
};

struct gpio_entry {
	struct gpiod_chip *chip;
	unsigned int offset;
	struct gpiod_line_request *request;
	ubx_port_t *port;
	int is_output;		/* 1: output GPIO (has in-port), 0: input GPIO (has out-port) */
	unsigned int last_val;	/* previous value, for emit_on_change */
	int last_valid;		/* 1 if last_val has been set */
};

struct gpio_info {
	const struct ubx_gpio_config *cfgs;
	long num_gpios;
	struct gpio_entry *entries;

	const struct ubx_triggee *trigee;
	long trigee_len;
};

/* Search all gpiochips for a line with the given name */
static int find_line(const char *name, struct gpiod_chip **chip_out, unsigned int *offset_out)
{
	char path[32];

	for (int i = 0; i < 16; i++) {
		snprintf(path, sizeof(path), "/dev/gpiochip%d", i);
		if (!gpiod_is_gpiochip_device(path))
			continue;

		struct gpiod_chip *chip = gpiod_chip_open(path);
		if (!chip)
			continue;

		int off = gpiod_chip_get_line_offset_from_name(chip, name);
		if (off >= 0) {
			*chip_out = chip;
			*offset_out = (unsigned int)off;
			return 0;
		}

		gpiod_chip_close(chip);
	}

	return -1;
}

static int gpio_init(ubx_block_t *b)
{
	long len;
	long i = 0;
	struct gpio_info *inf;

	b->private_data = calloc(1, sizeof(struct gpio_info));
	if (!b->private_data) {
		ubx_crit(b, "ENOMEM");
		return EOUTOFMEM;
	}
	inf = (struct gpio_info *)b->private_data;

	len = cfg_getptr_gpio_config(b, "gpios", &inf->cfgs);
	if (len <= 0) {
		ubx_err(b, "EINVALID_CONFIG: 'gpios' not set or invalid");
		goto out_free_inf;
	}
	inf->num_gpios = len;

	inf->entries = calloc(len, sizeof(struct gpio_entry));
	if (!inf->entries) {
		ubx_crit(b, "ENOMEM");
		goto out_free_inf;
	}

	for (i = 0; i < inf->num_gpios; i++) {
		const struct ubx_gpio_config *cfg = &inf->cfgs[i];
		struct gpio_entry *e = &inf->entries[i];
		struct gpiod_line_settings *settings;
		struct gpiod_line_config *lcfg;
		struct gpiod_request_config *rcfg;
		enum gpiod_line_direction gdir;
		int ret;

		if (cfg->name[0] == '\0') {
			ubx_err(b, "EINVALID_CONFIG: gpio[%ld]: empty name", i);
			goto out_cleanup;
		}

		if (cfg->dir == UBX_GPIO_DIR_OUT) {
			gdir = GPIOD_LINE_DIRECTION_OUTPUT;
			e->is_output = 1;
		} else if (cfg->dir == UBX_GPIO_DIR_IN) {
			gdir = GPIOD_LINE_DIRECTION_INPUT;
			e->is_output = 0;
		} else {
			ubx_err(b, "EINVALID_CONFIG: gpio '%s': invalid dir %d (0=in, 1=out)",
				cfg->name, (int)cfg->dir);
			goto out_cleanup;
		}

		if (find_line(cfg->name, &e->chip, &e->offset) < 0) {
			ubx_err(b, "EINVALID_CONFIG: gpio '%s': line not found", cfg->name);
			goto out_cleanup;
		}

		settings = gpiod_line_settings_new();
		if (!settings) {
			ubx_crit(b, "ENOMEM: gpio '%s'", cfg->name);
			gpiod_chip_close(e->chip);
			e->chip = NULL;
			goto out_cleanup;
		}

		gpiod_line_settings_set_direction(settings, gdir);
		gpiod_line_settings_set_active_low(settings, cfg->active_low != 0);

		if (cfg->pull == UBX_GPIO_PULL_UP)
			gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_PULL_UP);
		else if (cfg->pull == UBX_GPIO_PULL_DOWN)
			gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_PULL_DOWN);

		lcfg = gpiod_line_config_new();
		if (!lcfg) {
			ubx_crit(b, "ENOMEM: gpio '%s'", cfg->name);
			gpiod_line_settings_free(settings);
			gpiod_chip_close(e->chip);
			e->chip = NULL;
			goto out_cleanup;
		}

		if (gpiod_line_config_add_line_settings(lcfg, &e->offset, 1, settings) < 0) {
			ubx_err(b, "gpio '%s': failed to configure line settings", cfg->name);
			gpiod_line_config_free(lcfg);
			gpiod_line_settings_free(settings);
			gpiod_chip_close(e->chip);
			e->chip = NULL;
			goto out_cleanup;
		}
		gpiod_line_settings_free(settings);

		rcfg = gpiod_request_config_new();
		if (!rcfg) {
			ubx_crit(b, "ENOMEM: gpio '%s'", cfg->name);
			gpiod_line_config_free(lcfg);
			gpiod_chip_close(e->chip);
			e->chip = NULL;
			goto out_cleanup;
		}
		gpiod_request_config_set_consumer(rcfg, "ubx/gpio");

		e->request = gpiod_chip_request_lines(e->chip, rcfg, lcfg);
		gpiod_request_config_free(rcfg);
		gpiod_line_config_free(lcfg);

		if (!e->request) {
			ubx_err(b, "gpio '%s': failed to request line: %m", cfg->name);
			gpiod_chip_close(e->chip);
			e->chip = NULL;
			goto out_cleanup;
		}

		if (e->is_output)
			ret = ubx_inport_add(b, cfg->name, "GPIO output value", 0, "unsigned int", 1);
		else
			ret = ubx_outport_add(b, cfg->name, "GPIO input value", 0, "unsigned int", 1);

		if (ret < 0) {
			ubx_err(b, "gpio '%s': failed to add port", cfg->name);
			gpiod_line_request_release(e->request);
			e->request = NULL;
			gpiod_chip_close(e->chip);
			e->chip = NULL;
			goto out_cleanup;
		}

		e->port = ubx_port_get(b, cfg->name);
	}

	return 0;

out_cleanup:
	for (long j = 0; j < i; j++) {
		struct gpio_entry *e = &inf->entries[j];
		ubx_port_rm(b, inf->cfgs[j].name);
		gpiod_line_request_release(e->request);
		gpiod_chip_close(e->chip);
	}
	free(inf->entries);
out_free_inf:
	free(b->private_data);
	b->private_data = NULL;
	return -1;
}

static int gpio_start(ubx_block_t *b)
{
	struct gpio_info *inf = (struct gpio_info *)b->private_data;

	/* reset per-GPIO change tracking so the first value is always emitted */
	for (long i = 0; i < inf->num_gpios; i++)
		inf->entries[i].last_valid = 0;

	inf->trigee_len = cfg_getptr_triggee(b, "trigee", &inf->trigee);
	if (inf->trigee_len < 0) {
		ubx_err(b, "failed to retrieve 'trigee' config");
		return -1;
	}

	if (inf->trigee_len > 0 && inf->trigee->b == NULL) {
		ubx_err(b, "EINVALID_CONFIG: trigee block reference is NULL");
		return -1;
	}

	return 0;
}

static void gpio_stop(ubx_block_t *b)
{
	struct gpio_info *inf = (struct gpio_info *)b->private_data;
	inf->trigee = NULL;
	inf->trigee_len = 0;
}

static void gpio_cleanup(ubx_block_t *b)
{
	struct gpio_info *inf = (struct gpio_info *)b->private_data;

	for (long i = 0; i < inf->num_gpios; i++) {
		struct gpio_entry *e = &inf->entries[i];
		ubx_port_rm(b, inf->cfgs[i].name);
		gpiod_line_request_release(e->request);
		gpiod_chip_close(e->chip);
	}
	free(inf->entries);
	free(b->private_data);
}

/* trigger a single ubx_triggee respecting num_steps (-1=disabled, 0/1=once, N=N times) */
static void do_trigee(ubx_block_t *b, const struct ubx_triggee *t)
{
	int steps = (t->num_steps == -1) ? 0 : (t->num_steps <= 1 ? 1 : t->num_steps);

	for (int s = 0; s < steps; s++) {
		if (ubx_cblock_step(t->b) != 0)
			ubx_err(b, "trigee '%s': step failed", t->b->name);
	}
}

static void gpio_step(ubx_block_t *b)
{
	struct gpio_info *inf = (struct gpio_info *)b->private_data;

	/* read input GPIOs and emit on output ports */
	for (long i = 0; i < inf->num_gpios; i++) {
		struct gpio_entry *e = &inf->entries[i];
		const struct ubx_gpio_config *cfg = &inf->cfgs[i];

		if (e->is_output)
			continue;

		enum gpiod_line_value val = gpiod_line_request_get_value(e->request, e->offset);
		if (val == GPIOD_LINE_VALUE_ERROR) {
			ubx_err(b, "gpio '%s': read failed: %m", cfg->name);
			continue;
		}

		unsigned int uval = (val == GPIOD_LINE_VALUE_ACTIVE) ? 1u : 0u;

		if (!cfg->emit_on_change || !e->last_valid || uval != e->last_val) {
			write_uint(e->port, &uval);
			e->last_val = uval;
			e->last_valid = 1;
		}
	}

	/* optionally trigger a block between reading inputs and writing outputs */
	if (inf->trigee_len > 0)
		do_trigee(b, inf->trigee);

	/* read input ports and drive output GPIOs */
	for (long i = 0; i < inf->num_gpios; i++) {
		struct gpio_entry *e = &inf->entries[i];
		const struct ubx_gpio_config *cfg = &inf->cfgs[i];

		if (!e->is_output)
			continue;

		unsigned int uval = 0;
		long len = read_uint(e->port, &uval);
		if (len <= 0)
			continue;

		enum gpiod_line_value gval = uval ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
		if (gpiod_line_request_set_value(e->request, e->offset, gval) < 0)
			ubx_err(b, "gpio '%s': write failed: %m", cfg->name);
	}
}

ubx_proto_block_t gpio_comp = {
	.name = "ubx/gpio",
	.meta_data = gpio_meta,
	.type = BLOCK_TYPE_COMPUTATION,
	.configs = gpio_configs,
	.init = gpio_init,
	.start = gpio_start,
	.stop = gpio_stop,
	.cleanup = gpio_cleanup,
	.step = gpio_step,
};

int gpio_module_init(ubx_node_t *nd)
{
	if (ubx_type_register(nd, &gpio_config_type))
		return -1;
	return ubx_block_register(nd, &gpio_comp);
}

void gpio_module_cleanup(ubx_node_t *nd)
{
	ubx_type_unregister(nd, gpio_config_type.name);
	ubx_block_unregister(nd, "ubx/gpio");
}

UBX_MODULE_INIT(gpio_module_init)
UBX_MODULE_CLEANUP(gpio_module_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
