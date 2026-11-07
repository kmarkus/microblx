/*
 * ubx/pack and ubx/unpack: pass a set of ports between threads or
 * processes as one frame, via an lfb broadcast buffer in POSIX shm.
 *
 * ubx/pack creates one in-port per entry of its 'ports' config. Each
 * step it reads all of them into one frame (with a bit per port
 * telling whether it had new data) and publishes the frame to the shm
 * segment named by its 'packid'. ubx/unpack joins the segment of the
 * same 'packid', creates one out-port per packed port and each step
 * writes the ports of the oldest unread frame that had data.
 *
 * The segment is self-describing (see pack_layout.h), and pack_shm.h
 * gives other clients the same access.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* pack_shm.h: before any system header, also via a compile definition */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ubx.h"
#include "port_spec.h"
#include "pack_layout.h"
#include "pack_shm.h"

#define CFG_PORTS	"ports"
#define CFG_BUFFER_LEN	"buffer_len"
#define CFG_PACKID	"packid"
#define POUT_OVERRUNS	"overruns"


_Static_assert(UBX_PACK_ID_LEN == UBX_PORT_NAME_MAXLEN + 1, "id length");
_Static_assert(UBX_PACK_TYPE_LEN == UBX_TYPE_NAME_MAXLEN + 1, "type length");

def_cfg_getptr_fun(cfg_getptr_port_spec, struct ubx_port_spec)

struct pk_port {
	char id[UBX_PACK_ID_LEN];
	const ubx_type_t *type;
	uint32_t len;
	uint32_t offset;
	uint32_t size;
	ubx_port_t *port;
};

struct pk_info {
	uint32_t nports;
	struct pk_port *ports;
	uint32_t nadded;	/* ports added to the block */
	uint32_t frame_size;
	struct ubx_pack_layout *lay;	/* of the ports config */
	struct ubx_pack_shm s;

	/* unpack only */
	lfb_rd_t rd;
	uint8_t *frame;
	unsigned long overruns;
	ubx_port_t *p_overruns;
};

/* no empty elements, no dots: slashes become dots in the segment name */
static int path_ok(const char *path)
{
	size_t len = strlen(path);

	return len > 0 && path[0] != '/' && path[len - 1] != '/' &&
		strstr(path, "//") == NULL && strchr(path, '.') == NULL &&
		strchr(path, ':') == NULL;
}

/**
 * shm_name - resolve the packid config into a segment name
 *
 * pk is relative to the subsystem of the block, /a/pk to the root of
 * its node and node:/a/pk to the root of another one. As the segment
 * name is node.a.pk, neither these nor the node may contain dots.
 */
static int shm_name(ubx_block_t *b, char *buf, size_t size)
{
	int n;
	long len;
	const char *id, *path, *sep, *node = b->nd->name;
	char scope[UBX_BLOCK_NAME_MAXLEN + 2] = "";
	int nodelen = (int)strlen(node);

	len = cfg_getptr_char(b, CFG_PACKID, &id);

	if (len <= 0 || id[0] == '\0') {
		ubx_err(b, "EINVALID_CONFIG: mandatory config %s unset", CFG_PACKID);
		return EINVALID_CONFIG;
	}

	path = id;
	sep = strchr(id, ':');

	if (sep != NULL) {
		if (sep == id || sep[1] != '/') {
			ubx_err(b, "EINVALID_CONFIG: %s %s: expected node:/path",
				CFG_PACKID, id);
			return EINVALID_CONFIG;
		}

		node = id;
		nodelen = (int)(sep - id);
		path = sep + 2;
	} else if (id[0] == '/') {
		path = id + 1;
	} else if ((sep = strrchr(b->name, '/')) != NULL) {
		snprintf(scope, sizeof(scope), "%.*s/", (int)(sep - b->name), b->name);
	}

	if (!path_ok(path) || strchr(scope, '.') != NULL ||
	    memchr(node, '.', (size_t)nodelen) != NULL) {
		ubx_err(b, "EINVALID_CONFIG: %s %s: no dots in it, or in the node and "
			"subsystem, and no empty path elements", CFG_PACKID, id);
		return EINVALID_CONFIG;
	}

	n = snprintf(buf, size, "%.*s.%s%s", nodelen, node, scope, path);

	if (n < 0 || (size_t)n >= size) {
		ubx_err(b, "EINVALID_CONFIG: %s %s: name too long", CFG_PACKID, id);
		return EINVALID_CONFIG;
	}

	for (char *c = buf; *c != '\0'; c++) {
		if (*c == '/')
			*c = '.';
	}

	return 0;
}

/* validate one port spec and resolve its type */
static int spec_resolve(ubx_block_t *b, const struct ubx_port_spec *spec,
			struct pk_port *pp)
{
	if (strnlen(spec->id, sizeof(spec->id)) == sizeof(spec->id) ||
	    spec->id[0] == '\0') {
		ubx_err(b, "EINVALID_CONFIG: %s: invalid id", CFG_PORTS);
		return EINVALID_CONFIG;
	}

	/* unpack has this port, next to one port per id */
	if (strcmp(spec->id, POUT_OVERRUNS) == 0) {
		ubx_err(b, "EINVALID_CONFIG: %s: id %s is reserved",
			CFG_PORTS, spec->id);
		return EINVALID_CONFIG;
	}

	if (strnlen(spec->type, sizeof(spec->type)) == sizeof(spec->type)) {
		ubx_err(b, "EINVALID_CONFIG: %s.%s: invalid type", CFG_PORTS, spec->id);
		return EINVALID_CONFIG;
	}

	pp->type = ubx_type_get(b->nd, spec->type);

	if (pp->type == NULL) {
		ubx_err(b, "EINVALID_TYPE: %s.%s: unknown type '%s'",
			CFG_PORTS, spec->id, spec->type);
		return EINVALID_TYPE;
	}

	strcpy(pp->id, spec->id);
	pp->len = (spec->len == 0) ? 1 : spec->len;
	return 0;
}

static void pk_free(ubx_block_t *b)
{
	struct pk_info *inf = (struct pk_info *)b->private_data;

	if (inf == NULL)
		return;

	for (uint32_t i = 0; i < inf->nadded; i++)
		ubx_port_rm(b, inf->ports[i].id);

	if (inf->s.fd != -2)
		ubx_pack_close(&inf->s);

	free(inf->lay);
	free(inf->frame);
	free(inf->ports);
	free(inf);
	b->private_data = NULL;
}

static int pk_alloc(ubx_block_t *b, uint32_t nports)
{
	struct pk_info *inf = calloc(1, sizeof(struct pk_info));

	b->private_data = inf;

	if (inf == NULL)
		goto out_oom;

	inf->ports = calloc(nports, sizeof(struct pk_port));

	if (inf->ports == NULL)
		goto out_oom;

	inf->nports = nports;
	inf->s.fd = -2;		/* no segment yet */
	return 0;

out_oom:
	ubx_err(b, "EOUTOFMEM: failed to alloc state");
	pk_free(b);
	return EOUTOFMEM;
}

static int pk_cache_ports(ubx_block_t *b)
{
	struct pk_info *inf = (struct pk_info *)b->private_data;

	for (uint32_t i = 0; i < inf->nports; i++) {
		inf->ports[i].port = ubx_port_get(b, inf->ports[i].id);
		assert(inf->ports[i].port != NULL);
	}

	return 0;
}

/* resolve the ports config into inf->ports and inf->lay */
static int pk_resolve_ports(ubx_block_t *b, const struct ubx_port_spec *specs)
{
	int ret;
	struct pk_info *inf = (struct pk_info *)b->private_data;
	uint64_t off = UBX_PACK_FRAME_START(inf->nports);

	inf->lay = calloc(1, ubx_pack_layout_size(inf->nports));

	if (inf->lay == NULL) {
		ubx_err(b, "EOUTOFMEM: failed to alloc layout");
		return EOUTOFMEM;
	}

	inf->lay->version = UBX_PACK_LAYOUT_VERSION;
	inf->lay->nports = inf->nports;

	for (uint32_t i = 0; i < inf->nports; i++) {
		struct pk_port *pp = &inf->ports[i];

		ret = spec_resolve(b, &specs[i], pp);

		if (ret != 0)
			return ret;

		if ((uint64_t)pp->type->size * pp->len > UINT32_MAX - off - UBX_PACK_FRAME_ALIGN) {
			ubx_err(b, "EINVALID_CONFIG: frame too large");
			return EINVALID_CONFIG;
		}

		ubx_pack_layout_add(inf->lay, i, pp->id, pp->type->name, pp->len,
				    (uint64_t)pp->type->size, &off);
		pp->offset = inf->lay->ports[i].offset;
		pp->size = inf->lay->ports[i].size;
	}

	inf->frame_size = ubx_pack_frame_size(inf->lay);
	return 0;
}

/**
 * pk_join - create or join the segment with the layout of the ports config
 *
 * @param b block
 * @param name segment name
 * @param writer non-zero for ubx/pack
 * @return 0 or ubx error code
 */
static int pk_join(ubx_block_t *b, const char *name, int writer)
{
	int ret;
	const uint32_t *buflenp;
	lfb_word_t depth = 0;
	struct pk_info *inf = (struct pk_info *)b->private_data;

	if (cfg_getptr_uint32(b, CFG_BUFFER_LEN, &buflenp) > 0) {
		if (*buflenp < 1) {
			ubx_err(b, "EINVALID_CONFIG: %s must be >= 1 (got %u)",
				CFG_BUFFER_LEN, *buflenp);
			return EINVALID_CONFIG;
		}

		depth = (lfb_word_t)*buflenp + 1;
	}

	ret = ubx_pack_join(&inf->s, name, inf->lay, depth, writer);

	switch (ret) {
	case 0:
		return 0;
	case -EBUSY:
		ubx_err(b, "shm %s already has a writer", name);
		return EINVALID_CONFIG;
	case -EPROTO:
		ubx_err(b, "ETYPE_MISMATCH: shm %s: its ports differ", name);
		return ETYPE_MISMATCH;
	case -ERANGE:
		ubx_err(b, "EINVALID_CONFIG: shm %s: its %s differs", name, CFG_BUFFER_LEN);
		return EINVALID_CONFIG;
	case -EEXIST:
		ubx_err(b, "shm %s exists, and is no ubx/pack segment", name);
		return EINVALID_CONFIG;
	default:
		ubx_err(b, "failed to join shm %s: %s", name, strerror(-ret));
		return EINVALID_CONFIG;
	}
}

/*
 * pack
 */

char pack_meta[] =
	"{ doc='pack ports into frames of an lfb shm broadcast buffer', realtime=true }";

ubx_proto_config_t pack_config[] = {
	{ .name = CFG_PACKID, .type_name = "char", .min = 1,
	  .doc = "segment: pk (in this subsystem), /a/pk (of this node) or node:/a/pk" },
	{ .name = CFG_PORTS, .type_name = "struct ubx_port_spec", .min = 1,
	  .doc = "ports to pack: { id=<port name>, type=<ubx type>, len=<array len> }" },
	{ .name = CFG_BUFFER_LEN, .type_name = "uint32_t", .max = 1,
	  .doc = "frames a reader may lag behind: if unset any, and 1 if it creates the segment" },
	{ .name = "loglevel", .type_name = "int" },
	{ 0 },
};

/* in-ports are added at init from the ports config */
ubx_proto_port_t pack_ports[] = {
	{ 0 },
};

static int pack_init(ubx_block_t *b)
{
	int ret;
	long len;
	char name[LFB_SHM_NAME_MAX];
	const struct ubx_port_spec *specs;
	struct pk_info *inf;

	ret = shm_name(b, name, sizeof(name));

	if (ret != 0)
		return ret;

	len = cfg_getptr_port_spec(b, CFG_PORTS, &specs);

	if (len < 0)
		return (int)len;

	if (len == 0) {
		ubx_err(b, "EINVALID_CONFIG: mandatory config %s unset", CFG_PORTS);
		return EINVALID_CONFIG;
	}

	ret = pk_alloc(b, (uint32_t)len);

	if (ret != 0)
		return ret;

	inf = (struct pk_info *)b->private_data;
	ret = pk_resolve_ports(b, specs);

	if (ret != 0)
		goto out;

	for (uint32_t i = 0; i < inf->nports; i++) {
		const struct pk_port *pp = &inf->ports[i];

		ret = ubx_inport_add(b, pp->id, "packed input", 0, pp->type->name, pp->len);

		if (ret != 0) {
			ubx_err(b, "failed to add port %s (duplicate id?)", pp->id);
			goto out;
		}

		inf->nadded++;
	}

	ret = pk_join(b, name, 1);

	if (ret != 0)
		goto out;

	return 0;

out:
	pk_free(b);
	return ret;
}

static void pack_step(ubx_block_t *b)
{
	struct pk_info *inf = (struct pk_info *)b->private_data;
	lfb_t *lfb = lfb_shm_lfb(&inf->s.shm);
	uint8_t *slot = lfb_get_wslot(lfb);
	uint64_t *present = (uint64_t *)slot;

	memset(present, 0, UBX_PACK_PRESENT_WORDS(inf->nports) * sizeof(uint64_t));

	for (uint32_t i = 0; i < inf->nports; i++) {
		const struct pk_port *pp = &inf->ports[i];
		ubx_data_t d = {
			.type = pp->type,
			.len = pp->len,
			.data = slot + pp->offset,
		};
		long n = __port_read(pp->port, &d);

		if (n <= 0)
			continue;

		/* short array read: no stale bytes from an older frame */
		if ((unsigned long)n < pp->len)
			memset(slot + pp->offset + n * pp->type->size, 0,
			       (pp->len - n) * pp->type->size);

		present[i / 64] |= (uint64_t)1 << (i % 64);
	}

	lfb_commit_wslot(lfb);
}

static void pack_cleanup(ubx_block_t *b)
{
	pk_free(b);
}

ubx_proto_block_t pack_comp = {
	.name = "ubx/pack",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = pack_meta,
	.configs = pack_config,
	.ports = pack_ports,

	.init = pack_init,
	.start = pk_cache_ports,
	.step = pack_step,
	.cleanup = pack_cleanup,
};

/*
 * unpack
 */

char unpack_meta[] =
	"{ doc='unpack the frames of a ubx/pack block into ports', realtime=true }";

ubx_proto_config_t unpack_config[] = {
	{ .name = CFG_PACKID, .type_name = "char", .min = 1,
	  .doc = "segment: pk (in this subsystem), /a/pk (of this node) or node:/a/pk" },
	{ .name = CFG_PORTS, .type_name = "struct ubx_port_spec",
	  .doc = "optional: expected ports. Creates the segment if there is none" },
	{ .name = CFG_BUFFER_LEN, .type_name = "uint32_t", .max = 1,
	  .doc = "frames a reader may lag behind: if unset any, and 1 if it creates the segment" },
	{ .name = "loglevel", .type_name = "int" },
	{ 0 },
};

/* the unpacked out-ports are added at init from the layout */
ubx_proto_port_t unpack_ports[] = {
	{ .name = POUT_OVERRUNS, .out_type_name = "unsigned long",
	  .doc = "number of overruns (frames lost), output on change" },
	{ 0 },
};

/* validate the segment's layout and resolve it into inf->ports */
static int unpack_load_layout(ubx_block_t *b)
{
	int ret;
	struct pk_info *inf = (struct pk_info *)b->private_data;
	const char *name = inf->s.shm.name;
	const lfb_t *lfb = lfb_shm_lfb(&inf->s.shm);
	const struct ubx_pack_layout *lay = ubx_pack_layout_get(&inf->s.shm);
	uint64_t min_off;

	if (lay == NULL)
		return EINVALID_CONFIG;

	min_off = UBX_PACK_FRAME_START(lay->nports);

	for (uint32_t i = 0; i < lay->nports; i++) {
		const struct ubx_pack_layout_port *lp = &lay->ports[i];
		struct pk_port *pp = &inf->ports[i];
		struct ubx_port_spec spec = { .len = lp->len };

		if (strnlen(lp->id, sizeof(lp->id)) == sizeof(lp->id) ||
		    strnlen(lp->type, sizeof(lp->type)) == sizeof(lp->type) ||
		    lp->len == 0) {
			ubx_err(b, "shm %s: invalid layout entry %u", name, i);
			return EINVALID_CONFIG;
		}

		strcpy(spec.id, lp->id);
		strcpy(spec.type, lp->type);

		ret = spec_resolve(b, &spec, pp);

		if (ret != 0)
			return ret;

		if ((uint64_t)pp->type->size * pp->len != lp->size ||
		    lp->offset % UBX_PACK_FRAME_ALIGN != 0 || lp->offset < min_off ||
		    (uint64_t)lp->offset + lp->size > lfb->frame_size) {
			ubx_err(b, "shm %s: port %s: size/offset mismatch (type %s size %lu, len %u, size %u, offset %u)",
				name, lp->id, lp->type, pp->type->size, lp->len,
				lp->size, lp->offset);
			return ETYPE_MISMATCH;
		}

		pp->offset = lp->offset;
		pp->size = lp->size;
	}

	inf->frame_size = lfb->frame_size;
	return 0;
}

/* the segment exists and has the layout we find there */
static int unpack_open(ubx_block_t *b, const char *name)
{
	int ret;
	struct ubx_pack_shm s;
	struct pk_info *inf;

	ret = ubx_pack_open(&s, name);

	if (ret == -ENOENT || ret == -EAGAIN) {
		ubx_err(b, "shm %s not found: start its writer first, or set %s",
			name, CFG_PORTS);
		return EINVALID_CONFIG;
	} else if (ret != 0) {
		ubx_err(b, "shm %s: %s", name,
			(ret == -EPROTO) ? "no valid ubx/pack layout" : strerror(-ret));
		return EINVALID_CONFIG;
	}

	ret = pk_alloc(b, ubx_pack_layout_get(&s.shm)->nports);

	if (ret != 0) {
		ubx_pack_close(&s);
		return ret;
	}

	inf = (struct pk_info *)b->private_data;
	inf->s = s;
	return 0;
}

/* create the segment, or join it after checking it against our ports */
static int unpack_join(ubx_block_t *b, const char *name,
		       const struct ubx_port_spec *specs, uint32_t nports)
{
	int ret = pk_alloc(b, nports);

	if (ret != 0)
		return ret;

	ret = pk_resolve_ports(b, specs);

	if (ret == 0)
		ret = pk_join(b, name, 0);

	if (ret != 0)
		pk_free(b);

	return ret;
}

static int unpack_init(ubx_block_t *b)
{
	int ret;
	long len;
	char name[LFB_SHM_NAME_MAX];
	const struct ubx_port_spec *specs;
	struct pk_info *inf;

	ret = shm_name(b, name, sizeof(name));

	if (ret != 0)
		return ret;

	len = cfg_getptr_port_spec(b, CFG_PORTS, &specs);

	if (len < 0)
		return (int)len;

	ret = (len > 0) ? unpack_join(b, name, specs, (uint32_t)len) :
		unpack_open(b, name);

	if (ret != 0)
		return ret;

	inf = (struct pk_info *)b->private_data;
	ret = unpack_load_layout(b);

	if (ret != 0)
		goto out;

	for (uint32_t i = 0; i < inf->nports; i++) {
		const struct pk_port *pp = &inf->ports[i];

		ret = ubx_outport_add(b, pp->id, "unpacked output", 0,
				      pp->type->name, pp->len);

		if (ret != 0) {
			ubx_err(b, "failed to add port %s", pp->id);
			goto out;
		}

		inf->nadded++;
	}

	inf->frame = malloc(inf->frame_size);

	if (inf->frame == NULL) {
		ubx_err(b, "EOUTOFMEM: failed to alloc frame");
		ret = EOUTOFMEM;
		goto out;
	}

	lfb_seek(lfb_shm_lfb(&inf->s.shm), &inf->rd, LFB_NEWEST);
	return 0;

out:
	pk_free(b);
	return ret;
}

/*
 * a re-initialized pack recreates its segment: switch to the new one
 * if the layout is unchanged, else the unpack block must be re-inited
 */
static int unpack_reopen(ubx_block_t *b)
{
	int ret;
	struct ubx_pack_shm s;
	struct pk_info *inf = (struct pk_info *)b->private_data;
	const struct ubx_pack_layout *lay = ubx_pack_layout_get(&inf->s.shm);

	ret = ubx_pack_open(&s, inf->s.shm.name);

	if (ret != 0) {
		ubx_err(b, "shm %s gone: %s", inf->s.shm.name, strerror(-ret));
		return EINVALID_CONFIG;
	}

	if (!ubx_pack_layout_same(&s.shm, lay)) {
		ubx_err(b, "shm %s: layout changed, re-init this block", inf->s.shm.name);
		ubx_pack_close(&s);
		return ETYPE_MISMATCH;
	}

	ubx_pack_close(&inf->s);
	inf->s = s;
	return 0;
}

static int unpack_start(ubx_block_t *b)
{
	int ret;
	struct pk_info *inf = (struct pk_info *)b->private_data;

	if (lfb_shm_stale(&inf->s.shm)) {
		ret = unpack_reopen(b);

		if (ret != 0)
			return ret;
	}

	pk_cache_ports(b);
	inf->p_overruns = ubx_port_get(b, POUT_OVERRUNS);
	assert(inf->p_overruns != NULL);

	/* frames written while stopped are history, not backlog */
	lfb_seek(lfb_shm_lfb(&inf->s.shm), &inf->rd, LFB_NEWEST);
	return 0;
}

static void unpack_step(ubx_block_t *b)
{
	struct pk_info *inf = (struct pk_info *)b->private_data;
	int ret = lfb_read(&inf->rd, inf->frame);

	if (ret == -EPIPE) {
		inf->overruns++;
		write_ulong(inf->p_overruns, &inf->overruns);
		ret = lfb_read(&inf->rd, inf->frame);

		if (ret == -EPIPE) {
			inf->overruns++;
			write_ulong(inf->p_overruns, &inf->overruns);
		}
	}

	if (ret != 1)
		return;

	for (uint32_t i = 0; i < inf->nports; i++) {
		const struct pk_port *pp = &inf->ports[i];
		ubx_data_t d = {
			.type = pp->type,
			.len = pp->len,
			.data = inf->frame + pp->offset,
		};

		if (ubx_pack_present(inf->frame, i))
			__port_write(pp->port, &d);
	}
}

static void unpack_cleanup(ubx_block_t *b)
{
	pk_free(b);
}

ubx_proto_block_t unpack_comp = {
	.name = "ubx/unpack",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = unpack_meta,
	.configs = unpack_config,
	.ports = unpack_ports,

	.init = unpack_init,
	.start = unpack_start,
	.step = unpack_step,
	.cleanup = unpack_cleanup,
};

/*
 * module
 */

static int pack_mod_init(ubx_node_t *nd)
{
	if (ubx_block_register(nd, &pack_comp) != 0)
		return -1;

	if (ubx_block_register(nd, &unpack_comp) != 0) {
		ubx_block_unregister(nd, "ubx/pack");
		return -1;
	}

	return 0;
}

static void pack_mod_cleanup(ubx_node_t *nd)
{
	ubx_block_unregister(nd, "ubx/unpack");
	ubx_block_unregister(nd, "ubx/pack");
}

UBX_MODULE_INIT(pack_mod_init)
UBX_MODULE_CLEANUP(pack_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
