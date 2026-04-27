/*
 * GPS block using libgps (gpsd shared memory interface)
 * SPDX-License-Identifier: BSD-3-Clause
 */

#undef UBX_DEBUG

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <gps.h>

#include "ubx.h"

#include "types/gps_data.h"
#include "types/gps_data.h.hexarr"

ubx_type_t gps_data_type = def_struct_type(struct ubx_gps_data, &gps_data_h);
def_type_accessors(gps_data, struct ubx_gps_data)

char gps_meta[] = "{ doc='GPS block: reads position/velocity from gpsd via shared memory' }";

ubx_proto_config_t gps_configs[] = {
	{ .name = "loglevel", .type_name = "int" },
	{ 0 },
};

#define PORT_GPS "gps"

ubx_proto_port_t gps_ports[] = {
	{ .name = PORT_GPS,
	  .out_type_name = "struct ubx_gps_data",
	  .out_data_len = 1,
	  .doc = "GPS fix: position, velocity, fix quality" },
	{ 0 },
};

struct gps_inf {
	struct gps_data_t gpsdata;
	ubx_port_t *p_gps;
};

static int gps_init(ubx_block_t *b)
{
	b->private_data = calloc(1, sizeof(struct gps_inf));
	if (!b->private_data) {
		ubx_crit(b, "ENOMEM");
		return EOUTOFMEM;
	}
	struct gps_inf *inf = (struct gps_inf *)b->private_data;

	if (gps_open(GPSD_SHARED_MEMORY, "0", &inf->gpsdata) != 0) {
		ubx_err(b, "failed to open gpsd shared memory: %s", gps_errstr(errno));
		free(b->private_data);
		b->private_data = NULL;
		return -1;
	}

	inf->p_gps = ubx_port_get(b, PORT_GPS);
	return 0;
}

static void gps_cleanup(ubx_block_t *b)
{
	struct gps_inf *inf = (struct gps_inf *)b->private_data;
	gps_close(&inf->gpsdata);
	free(b->private_data);
}

static void gps_step(ubx_block_t *b)
{
	struct gps_inf *inf = (struct gps_inf *)b->private_data;

	if (!gps_waiting(&inf->gpsdata, 0))
		return;

	if (gps_read(&inf->gpsdata, NULL, 0) < 0) {
		ubx_err(b, "gps_read failed: %s", gps_errstr(errno));
		return;
	}

	const struct gps_fix_t *fix = &inf->gpsdata.fix;

	struct ubx_gps_data out;
	out.time               = (int64_t)fix->time.tv_sec * INT64_C(1000000000) + fix->time.tv_nsec;
	out.latitude           = fix->latitude;
	out.longitude          = fix->longitude;
	out.altMSL             = fix->altMSL;
	out.altHAE             = fix->altHAE;
	out.speed              = fix->speed;
	out.track              = fix->track;
	out.climb              = fix->climb;
	out.eph                = fix->eph;
	out.epv                = fix->epv;
	out.mode               = fix->mode;
	out.status             = fix->status;
	out.satellites_used    = inf->gpsdata.satellites_used;
	out.satellites_visible = inf->gpsdata.satellites_visible;

	write_gps_data(inf->p_gps, &out);
}

ubx_proto_block_t gps_comp = {
	.name      = "ubx/gps",
	.meta_data = gps_meta,
	.type      = BLOCK_TYPE_COMPUTATION,
	.configs   = gps_configs,
	.ports     = gps_ports,
	.init      = gps_init,
	.cleanup   = gps_cleanup,
	.step      = gps_step,
};

int gps_module_init(ubx_node_t *nd)
{
	if (ubx_type_register(nd, &gps_data_type))
		return -1;
	return ubx_block_register(nd, &gps_comp);
}

void gps_module_cleanup(ubx_node_t *nd)
{
	ubx_type_unregister(nd, gps_data_type.name);
	ubx_block_unregister(nd, "ubx/gps");
}

UBX_MODULE_INIT(gps_module_init)
UBX_MODULE_CLEANUP(gps_module_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
