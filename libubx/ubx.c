/*
 * microblx: embedded, realtime safe, reflective function blocks.
 *
 * Copyright (C) 2013,2014 Markus Klotzbuecher
 *                    <markus.klotzbuecher@mech.kuleuven.be>
 * Copyright (C) 2014-2020 Markus Klotzbuecher <mk@mkio.de>
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#undef UBX_DEBUG

#include "ubx.h"
#include <config.h>

/* core logging helpers */
#define CORE_LOG_SRC			"ubxcore"

#define log_emerg(nd, fmt, ...)		ubx_log(UBX_LOGLEVEL_EMERG,  nd, CORE_LOG_SRC, fmt, ##__VA_ARGS__)
#define log_alert(nd, fmt, ...)		ubx_log(UBX_LOGLEVEL_ALERT,  nd, CORE_LOG_SRC, fmt, ##__VA_ARGS__)
#define log_crit(nd,  fmt, ...)		ubx_log(UBX_LOGLEVEL_CRIT,   nd, CORE_LOG_SRC, fmt, ##__VA_ARGS__)
#define log_err(nd, fmt, ...)		ubx_log(UBX_LOGLEVEL_ERR,    nd, CORE_LOG_SRC, fmt, ##__VA_ARGS__)
#define log_warn(nd, fmt, ...)		ubx_log(UBX_LOGLEVEL_WARN,   nd, CORE_LOG_SRC, fmt, ##__VA_ARGS__)
#define log_notice(nd, fmt, ...)	ubx_log(UBX_LOGLEVEL_NOTICE, nd, CORE_LOG_SRC, fmt, ##__VA_ARGS__)
#define log_info(nd, fmt, ...)		ubx_log(UBX_LOGLEVEL_INFO,   nd, CORE_LOG_SRC, fmt, ##__VA_ARGS__)

#define logf_emerg(nd, fmt, ...)	ubx_log(UBX_LOGLEVEL_EMERG,  nd, __func__, fmt, ##__VA_ARGS__)
#define logf_alert(nd, fmt, ...)	ubx_log(UBX_LOGLEVEL_ALERT,  nd, __func__, fmt, ##__VA_ARGS__)
#define logf_crit(nd,  fmt, ...)	ubx_log(UBX_LOGLEVEL_CRIT,   nd, __func__, fmt, ##__VA_ARGS__)
#define logf_err(nd, fmt, ...)		ubx_log(UBX_LOGLEVEL_ERR,    nd, __func__, fmt, ##__VA_ARGS__)
#define logf_warn(nd, fmt, ...)		ubx_log(UBX_LOGLEVEL_WARN,   nd, __func__, fmt, ##__VA_ARGS__)
#define logf_notice(nd, fmt, ...)	ubx_log(UBX_LOGLEVEL_NOTICE, nd, __func__, fmt, ##__VA_ARGS__)
#define logf_info(nd, fmt, ...)		ubx_log(UBX_LOGLEVEL_INFO,   nd, __func__, fmt, ##__VA_ARGS__)

#ifdef UBX_DEBUG
# define log_debug(nd, fmt, ...)	ubx_log(UBX_LOGLEVEL_DEBUG,  nd, CORE_LOG_SRC, fmt, ##__VA_ARGS__)
# define logf_debug(nd, fmt, ...)	ubx_log(UBX_LOGLEVEL_DEBUG,  nd, __func__, fmt, ##__VA_ARGS__)
#else
# define log_debug(nd, fmt, ...)	do {} while (0)
# define logf_debug(nd, fmt, ...)	do {} while (0)
#endif

/* predicates */
int blk_is_proto(const ubx_block_t *b) { return b->prototype == NULL; }
int blk_is_instance(const ubx_block_t *b) { return !blk_is_proto(b); }

int port_is_out(const ubx_port_t *p) { return p->out_type != NULL; }
int port_is_in(const ubx_port_t *p) { return p->in_type != NULL; }
int port_is_inout(const ubx_port_t *p) { return port_is_out(p) && port_is_in(p); }
int port_is_cloned(const ubx_port_t *p) { return p->attrs & PORT_ATTR_CLONED; }
int port_is_dyn(const ubx_port_t *p) { return !port_is_cloned(p); }

int cfg_is_cloned(const ubx_config_t *c) { return c->attrs & CONFIG_ATTR_CLONED; }
int cfg_is_dyn(const ubx_config_t *c) { return !cfg_is_cloned(c); }



/* for pretty printing */
const char *block_states[] = {	"preinit", "inactive", "active" };

/**
 * Convert block state to string.
 *
 * @param state
 *
 * @return const char pointer to string name.
 */
const char *block_state_tostr(unsigned int state)
{
	if (state >= sizeof(block_states))
		return "invalid";
	return block_states[state];
}


/**
 * get_typename - return the type-name of the given data.
 *
 * @param data
 *
 * @return type name
 */
const char *get_typename(const ubx_data_t *data)
{
	if (data && data->type)
		return data->type->name;
	return NULL;
}

/**
 * ubx_module_load - load a module in a node.
 *
 * @param ni
 * @param lib
 *
 * @return 0 if Ok, non-zero otherwise.
 */
int ubx_module_load(ubx_node_t *nd, const char *lib)
{
	int ret = -1;
	char *err;
	ubx_module_t *mod;

	HASH_FIND_STR(nd->modules, lib, mod);

	if (mod != NULL) {
		logf_err(nd, "module %s already loaded", lib);
		goto out;
	}

	/* allocate data */
	mod = calloc(sizeof(ubx_module_t), 1);

	if (mod == NULL) {
		logf_err(nd, "failed to alloc module data");
		goto out;
	}

	mod->id = strdup(lib);

	if (mod->id == NULL) {
		logf_err(nd, "cloning mod name failed");
		goto out_err_free_mod;
	}
#ifdef UBX_CONFIG_VALGRIND
	mod->handle = dlopen(lib, RTLD_NOW | RTLD_NODELETE);
#else
	mod->handle = dlopen(lib, RTLD_NOW);
#endif
	if (mod->handle == NULL) {
		ubx_log(UBX_LOGLEVEL_ERR, nd, "dlopen", "%s", dlerror());
		goto out_err_free_id;
	}

	dlerror();

	mod->init = dlsym(mod->handle, "__ubx_initialize_module");
	err = dlerror();
	if (err != NULL)  {
		logf_err(nd, "no module_init for mod %s found: %s", lib, err);
		goto out_err_close;
	}

	dlerror();

	mod->cleanup = dlsym(mod->handle, "__ubx_cleanup_module");
	err = dlerror();
	if (err != NULL)  {
		logf_err(nd, "no module_cleanup found for %s: %s", lib, err);
		goto out_err_close;
	}

	dlerror();

	mod->spdx_license_id = dlsym(mod->handle, "__ubx_module_license_spdx");
	err = dlerror();
	if (err != NULL)
		logf_warn(nd, "missing UBX_MODULE_LICENSE_SPDX in module %s", lib);

	/* execute module init */
	if (mod->init(nd) != 0) {
		logf_err(nd, "module_init of %s failed", lib);
		goto out_err_close;
	}

	/* register with node */
	HASH_ADD_KEYPTR(hh, nd->modules, mod->id, strlen(mod->id), mod);

	logf_debug(nd, "loaded %s", lib);
	ret = 0;
	goto out;

 out_err_close:
	dlclose(mod->handle);
 out_err_free_id:
	free((char *)mod->id);
 out_err_free_mod:
	free(mod);
 out:
	return ret;
}

ubx_module_t *ubx_module_get(ubx_node_t *nd, const char *lib)
{
	ubx_module_t *mod = NULL;

	HASH_FIND_STR(nd->modules, lib, mod);
	return mod;
}

/**
 * ubx_module_close - close a module from a node.
 *
 * @param ni node_info
 * @param lib name of module library to unload
 */
static void ubx_module_cleanup(ubx_node_t *nd, const char *lib)
{
	ubx_module_t *mod;

	logf_debug(nd, "calling module cleanup %s", lib);

	HASH_FIND_STR(nd->modules, lib, mod);

	if (mod == NULL) {
		logf_err(nd, "unknown module %s", lib);
		return;
	}

	mod->cleanup(nd);
}

static void ubx_module_close(ubx_node_t *nd, const char *lib)
{
	ubx_module_t *mod;

	logf_debug(nd, "closing module %s", lib);

	HASH_FIND_STR(nd->modules, lib, mod);

	if (mod == NULL) {
		logf_err(nd, "unknown module %s", lib);
		return;
	}

	HASH_DEL(nd->modules, mod);

	dlclose(mod->handle);
	free((char *)mod->id);
	free(mod);
}

/**
 * ubx_module_unload - unload a module from a node.
 *
 * @param ni node_info
 * @param lib name of module library to unload
 */
void ubx_module_unload(ubx_node_t *nd, const char *lib)
{
	ubx_module_cleanup(nd, lib);
	ubx_module_close(nd, lib);
}


/**
 * initalize node_info
 *
 * @param ni
 *
 * @return 0 if ok, -1 otherwise.
 */
int ubx_node_init(ubx_node_t *nd, const char *name, uint32_t attrs)
{
	int ret = -1;

	nd->loglevel = (nd->loglevel == 0) ?
		UBX_LOGLEVEL_DEFAULT : nd->loglevel;

	if (ubx_log_init(nd)) {
		fprintf(stderr, "Error: failed to initalize logging.");
		goto out;
	}

	if (name == NULL) {
		logf_err(nd, "node name is NULL");
		goto out;
	}

	logf_notice(nd, "node_init: %s, loglevel: %u", name, nd->loglevel);

	strncpy((char*)nd->name, name, UBX_NODE_NAME_MAXLEN);

	if (attrs & ND_DUMPABLE) {
		if (prctl(PR_SET_DUMPABLE, 1, 0, 0, 0) != 0) {
			logf_err(nd, "enabling core dumps PR_SET_DUMPABLE): %m");
			goto out;
		}
		logf_info(nd, "core dumps enabled (PR_SET_DUMPABLE)");
	}

	if (attrs & ND_MLOCK_ALL) {
		if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
			logf_err(nd, "mlockall failed CUR|FUT: %m");
			goto out;
		};
		logf_info(nd, "mlockall CUR|FUT succeeded");
	}

#ifdef TIMESRC_TSC
	logf_info(nd, "TSC timesource enabled");
#endif

	nd->attrs = attrs;
	nd->blocks = NULL;
	nd->types = NULL;
	nd->modules = NULL;
	nd->cur_seqid = 0;

	ret = 0;
 out:
	return ret;
}

/**
 * ubx_node_clear
 *
 * Stop, cleanup and remove all block instances of the given node.
 *
 * @param ni
 */
void ubx_node_clear(ubx_node_t *nd)
{
	ubx_block_t *b = NULL, *btmp = NULL;

	logf_debug(nd, "node %s", nd->name);

	/* stop all blocks */
	HASH_ITER(hh, nd->blocks, b, btmp) {
		if (b->block_state == BLOCK_STATE_ACTIVE) {
			logf_debug(nd, "stopping block %s", b->name);
			if (ubx_block_stop(b) != 0)
				logf_err(nd, "failed to stop block %s", b->name);
		}
	}

	/* cleanup all blocks */
	HASH_ITER(hh, nd->blocks, b, btmp) {
		if (b->block_state == BLOCK_STATE_INACTIVE) {
			logf_debug(nd, "cleaning up block %s", b->name);
			if (ubx_block_cleanup(b) != 0)
				logf_err(nd, "failed to cleanup block %s", b->name);
		}
	}

	/* rm all non prototype blocks */
	HASH_ITER(hh, nd->blocks, b, btmp) {
		if (b->block_state == BLOCK_STATE_PREINIT && blk_is_instance(b)) {
			logf_debug(nd, "removing block %s", b->name);
			if (ubx_block_rm(nd, b->name) != 0)
				logf_err(nd, "ubx_block_rm failed for %s", b->name);
		}
	}
}

/**
 * ubx_node_cleanup - cleanup a node
 *
 * This function will run ubx_node_clear and then unload all
 * modules. The node is empty but still valid afterwards.
 *
 * @param ni
 */
void ubx_node_cleanup(ubx_node_t *nd)
{
	int cnt;
	ubx_module_t *m = NULL, *mtmp = NULL;

	logf_debug(nd, "node %s", nd->name);

	ubx_node_clear(nd);

	/* cleanup all modules */
	HASH_ITER(hh, nd->modules, m, mtmp)
		ubx_module_cleanup(nd, m->id);

	cnt = ubx_num_types(nd);
	if (cnt > 0)
		logf_warn(nd, "not has types after cleanup");

	cnt = ubx_num_blocks(nd);
	if (cnt > 0)
		logf_warn(nd, "%d blocks after cleanup", cnt);

	/* close all modules */
	HASH_ITER(hh, nd->modules, m, mtmp)
		ubx_module_close(nd, m->id);

	cnt = ubx_num_modules(nd);
	if (cnt > 0)
		logf_warn(nd, "%d modules after cleanup", cnt);

	nd->cur_seqid = 0;
}

/**
 * ubx_node_rm - cleanup an destroy a node
 *
 * calls ubx_node_cleanup and frees node member memory. Must be
 * reinitialized with ubx_node_init before reusing.
 *
 * @param ni
 */
void ubx_node_rm(ubx_node_t *nd)
{
	logf_info(nd, "removing node %s", nd->name);
	ubx_node_cleanup(nd);
	ubx_log_cleanup(nd);
	memset((char*) nd->name, 0, UBX_NODE_NAME_MAXLEN);
}

int ubx_block_check(ubx_node_t *nd, ubx_block_t *b)
{
	if (!b) {
		logf_err(nd, "NULL block");
		return EINVALID_BLOCK;
	}

	if (b->type != BLOCK_TYPE_COMPUTATION &&
	    b->type != BLOCK_TYPE_INTERACTION) {
		logf_err(nd, "invalid block type %d", b->type);
		return EINVALID_BLOCK;
	}
	return 0;
}

/**
 * __block_register - register a block with a node
 *
 * @ni: node to register with
 * @block: block to register
 */
static int __block_register(ubx_node_t *nd, ubx_block_t *block)
{
	ubx_block_t *tmpc;

	if (ubx_block_check(nd, block))
		return -1;

	HASH_FIND_STR(nd->blocks, block->name, tmpc);

	if (tmpc != NULL) {
		log_err(nd, "a block named %s is already registered", block->name);
		return -1;
	}

	HASH_ADD_KEYPTR(hh, nd->blocks, block->name, strlen(block->name), block);

	block->nd = nd;

	logf_debug(nd, "registered %s", block->name);

	return 0;
}


/**
 * ubx_block_register - register a prototype block with the given
 * node_info. In contrast to runtime blocks, this expects simple { 0 }
 * terminated arrays for configs and ports.
 *
 * @param ni
 * @param block
 *
 * @return 0 if Ok, < 0 otherwise.
 */
int ubx_block_register(ubx_node_t *nd, struct ubx_proto_block *prot)
{
	int ret;
	struct ubx_proto_port *p;
	struct ubx_proto_config *c;
	ubx_block_t *newb;

	newb = calloc(1, sizeof(ubx_block_t));

	if (newb == NULL) {
		logf_err(nd, "EOUTOFMEM");
		return EOUTOFMEM;
	}

	newb->block_state = BLOCK_STATE_PREINIT;

	if(strlen(prot->name) > UBX_BLOCK_NAME_MAXLEN) {
		logf_err(nd, "block name %s too long (max: %u)",
			 prot->name, UBX_BLOCK_NAME_MAXLEN);
		ret = EINVALID_BLOCK;
		goto out_err;
	}

	strncpy((char*) newb->name, prot->name, UBX_BLOCK_NAME_MAXLEN);

	newb->type = prot->type;
	newb->attrs = prot->attrs;
	newb->prototype = NULL;

	/* needs to be set here for ubx_port/config_add */
	newb->nd = nd;

	if (prot->meta_data)
		newb->meta_data = strdup(prot->meta_data);

	newb->init = prot->init;
	newb->start = prot->start;
	newb->stop = prot->stop;
	newb->cleanup = prot->cleanup;

	switch (prot->type) {
	case BLOCK_TYPE_COMPUTATION:
		newb->step = prot->step;
		break;
	case BLOCK_TYPE_INTERACTION:
		newb->read = prot->read;
		newb->write = prot->write;
		break;
	}

	if (prot->ports) {
		for (p = prot->ports; p->name != NULL; p++) {
			ret = ubx_port_add(newb,
					   p->name,
					   p->doc,
					   p->attrs | PORT_ATTR_CLONED,
					   p->in_type_name,
					   p->in_data_len,
					   p->out_type_name,
					   p->out_data_len);
			if (ret)
				goto out_err;
		}
	}

	if (prot->configs != NULL) {
		for (c = prot->configs; c->name != NULL; c++) {
			ret = ubx_config_add2(newb,
					      c->name,
					      c->doc,
					      c->type_name,
					      c->min,
					      c->max,
					      c->attrs | CONFIG_ATTR_CLONED);
			if (ret)
				goto out_err;
		}
	}

	return __block_register(nd, newb);
out_err:
	ubx_block_free(newb);
	return ret;
}

/**
 * Retrieve a block by name
 *
 * @param ni
 * @param name
 *
 * @return ubx_block_t*
 */
ubx_block_t *ubx_block_get(ubx_node_t *nd, const char *name)
{
	ubx_block_t *tmpc = NULL;

	if (name == NULL)
		goto out;

	HASH_FIND_STR(nd->blocks, name, tmpc);
out:
	return tmpc;
}

/**
 * ubx_block_unregister - unregister a block and free its memory.
 *
 * @param ni
 * @param type
 * @param name
 *
 * @return 0 if OK, -1 otherwise
 */
int ubx_block_unregister(ubx_node_t *nd, const char *name)
{
	ubx_block_t *tmpc;

	HASH_FIND_STR(nd->blocks, name, tmpc);

	if (tmpc == NULL) {
		logf_err(nd, "block %s not registered", name);
		return -1;
	}

	logf_debug(nd, "unregistering %s block %s",
		   (blk_is_proto(tmpc) ? "prototype" : "instance"), name);

	HASH_DEL(nd->blocks, tmpc);
	ubx_block_free(tmpc);
	return 0;
}


/**
 * ubx_type_register - register a type with a node.
 *
 * @param ni
 * @param type
 *
 * @return
 */
int ubx_type_register(ubx_node_t *nd, ubx_type_t *type)
{
	ubx_type_t *tmp;

	if (type == NULL) {
		logf_err(nd, "type is NULL");
		return EINVALID_TYPE;
	}

	if (type->name == NULL) {
		logf_err(nd, "type name is NULL");
		return EINVALID_TYPE;
	}

	if (strlen(type->name) > UBX_TYPE_NAME_MAXLEN) {
		logf_err(nd, "type name %s too long (max: %u)",
			 type->name, UBX_TYPE_NAME_MAXLEN);
		return EINVALID_TYPE;
	}

	/* check that its not already registered */
	HASH_FIND_STR(nd->types, type->name, tmp);

	if (tmp != NULL) {
		/* check if types are the same, if yes no error */
		logf_warn(nd, "%s already registered.", tmp->name);
		return EALREADY_REGISTERED;
	}

	type->seqid = nd->cur_seqid++;
	type->nd = nd;

	/* compute md5 fingerprint for type */
	md5((const unsigned char *)type->name, strlen(type->name), type->hash);

	HASH_ADD_KEYPTR(hh, nd->types, type->name, strlen(type->name), type);

	logf_debug(nd, "registered type %s: seqid %lu, ptr %p",
		   type->name, type->seqid, type);
	return 0;
}

/**
 * ubx_type_unregister - unregister type with node
 *
 * Should we add use count handling and only succeed unloading when
 * not used?
 *
 * @param ni
 * @param name
 *
 * @return type
 */
ubx_type_t *ubx_type_unregister(ubx_node_t *nd, const char *name)
{
	ubx_type_t *ret;

	HASH_FIND_STR(nd->types, name, ret);

	if (ret == NULL) {
		logf_err(nd, "type %s not registered", name);
		goto out;
	}

	HASH_DEL(nd->types, ret);

 out:
	return ret;
}

/**
 * ubx_type_get - find a ubx_type by name.
 *
 * @param ni
 * @param name
 *
 * @return pointer to ubx_type or NULL if not found.
 */
ubx_type_t *ubx_type_get(ubx_node_t *nd, const char *name)
{
	ubx_type_t *type = NULL;

	if (name == NULL) {
		logf_debug(nd, "name argument is NULL");
		goto out;
	}

	HASH_FIND_STR(nd->types, name, type);
out:
	return type;
}

/**
 * ubx_type_get_by_hash
 *
 * @param ni node_info
 * @param hash binary hash array of TYPE_HASH_LEN+1 size
 * @return ubx_type_t* or NULL if not found
 */
ubx_type_t *ubx_type_get_by_hash(ubx_node_t *nd, const uint8_t *hash)
{
	ubx_type_t *type = NULL, *tmptype = NULL;

	HASH_ITER(hh, nd->types, type, tmptype) {
		if (strncmp((char *)type->hash,
			    (char *)hash, UBX_TYPE_HASH_LEN) == 0)
			return type;
	}
	return NULL;
}

/**
 * ubx_type_get_by_hash
 *
 * lookup a ubx type by its type hash
 * @param ni
 * @param hashstr zero terminated hex string of TYPE_HASHSTR_LEN+1 size
 * @return ubx_type_t* or NULL if not found
 */
ubx_type_t *ubx_type_get_by_hashstr(ubx_node_t *nd, const char *hashstr)
{
	int len;
	char tmp[3];
	uint8_t hash[UBX_TYPE_HASH_LEN+1];

	len = strlen(hashstr);

	if (len != UBX_TYPE_HASHSTR_LEN) {
		logf_err(nd, "invalid length of hashstr %u", len);
		return NULL;
	}

	/* convert from string to binary array */
	for (int i = 0; i < UBX_TYPE_HASH_LEN; i++) {
		tmp[0] = hashstr[i * 2];
		tmp[1] = hashstr[i * 2 + 1];
		tmp[2] = '\0';
		hash[i] = (uint8_t) strtoul(tmp, NULL, 16);
	}
	hash[UBX_TYPE_HASH_LEN] = '\0';

	return ubx_type_get_by_hash(nd, hash);
}


/**
 * ubx_type_hashstr
 *
 * Convert the type hash to an hexstring
 *
 * @param t type
 * @param buf buffer of at least TYPE_HASHSTR_LEN + 1
 */
void ubx_type_hashstr(const ubx_type_t *t, char *buf)
{
	for (int i = 0; i < UBX_TYPE_HASH_LEN; i++)
		sprintf(buf + i * 2, "%02x", t->hash[i]);
	buf[UBX_TYPE_HASH_LEN * 2] = '\0';
}


/**
 * Allocate a ubx_data_t of the given type and array length.
 *
 * This type should be free'd using the ubx_data_free function.
 *
 * @type: ubx_type_t to be allocated
 * @array_len: array length of data. Can be 0.
 *
 * @return ubx_data_t* or NULL in case of error.
 */
ubx_data_t *__ubx_data_alloc(const ubx_type_t *typ, const long array_len)
{
	ubx_data_t *d = NULL;

	if (typ == NULL)
		goto out;

	d = calloc(1, sizeof(ubx_data_t));

	if (d == NULL)
		goto out;

	if (array_len > 0) {
		d->data = calloc(array_len, typ->size);
		if (d->data == NULL)
			goto out_free;
	}

	d->type = typ;
	d->len = array_len;

	/* all ok */
	goto out;

out_free:
	free(d);
	d = NULL;
out:
	return d;
}

/**
 * Allocate a ubx_data_t of the given type and array length.
 *
 * This type should be free'd using the ubx_data_free function.
 *
 * @param ni
 * @param typename
 * @param array_len
 *
 * @return ubx_data_t* or NULL in case of error.
 */
ubx_data_t *ubx_data_alloc(ubx_node_t *nd,
			   const char *typname,
			   long array_len)
{
	ubx_type_t *t = NULL;
	ubx_data_t *d = NULL;

	if (nd == NULL) {
		ERR("node_info NULL");
		goto out;
	}

	t = ubx_type_get(nd, typname);
	if (t == NULL) {
		logf_err(nd, "unknown type %s", typname);
		goto out;
	}

	d = __ubx_data_alloc(t, array_len);

	if (!d)
		logf_err(nd, "failed to alloc type %s[%ld]", typname, array_len);
 out:
	return d;
}

int ubx_data_resize(ubx_data_t *d, long newlen)
{
	int ret = EOUTOFMEM;
	void *ptr;
	unsigned int newsz = newlen * d->type->size;

	ptr = realloc(d->data, newsz);
	if (ptr == NULL)
		goto out;

	d->data = ptr;
	d->len = newlen;
	ret = 0;
 out:
	return ret;
}


/**
 * Free a previously allocated ubx_data_t type.
 *
 * @param ni
 * @param d
 */
void ubx_data_free(ubx_data_t *d)
{
	d->refcnt--;

	if (d->refcnt < 0) {
		free(d->data);
		free(d);
	}
}


/**
 * Calculate the size in bytes of a ubx_data_t buffer.
 *
 * @param d
 *
 * @return length in bytes if OK, <=0 otherwise
 */
long data_size(const ubx_data_t *d)
{
	if (d == NULL)
		return EINVALID_TYPE;

	if (d->type == NULL)
		return EINVALID_TYPE;

	return d->len * d->type->size;
}

int ubx_num_blocks(ubx_node_t *nd)
{
	return HASH_COUNT(nd->blocks);
}

int ubx_num_types(ubx_node_t *nd)
{
	return HASH_COUNT(nd->types);
}

int ubx_num_modules(ubx_node_t *nd)
{
	return HASH_COUNT(nd->modules);
}

/**
 * ubx_port_free - free port data
 *
 * @param p port pointer
 */
void ubx_port_free(ubx_port_t *p)
{
	if (p->in_interaction) free((struct ubx_block_t *)p->in_interaction);
	if (p->out_interaction) free((struct ubx_block_t *)p->out_interaction);
	if (p->doc) free((char *)p->doc);
	free(p);
}

/**
 * ubx_config_free_data - free a config's extra memory
 *
 * @param c config whose data to free
 */
static void ubx_config_free(ubx_config_t *c)
{
	if (c->doc) free((char *)c->doc);
	if (c->value) ubx_data_free(c->value);
	free(c);
}


/**
 * ubx_config_assign - assign a ubx_data
 *
 * this will increment the ubx_data refcnt.
 *
 * @param config
 * @param data ubx_data_t to assign
 *
 * @return 0 if OK, ETYPE_MISMATCH fpr mismatching types
 */
int ubx_config_assign(ubx_config_t *c, ubx_data_t *d)
{
	if (c->type != d->type)
		return ETYPE_MISMATCH;

	if (c->value)
		ubx_data_free(c->value);

	d->refcnt++;
	c->value = d;
	return 0;
}

/**
 * ubx_block_free - free all memory related to a block
 *
 * The block should have been previously unregistered.
 *
 * @param block_type
 * @param name
 *
 * @return
 */
void ubx_block_free(ubx_block_t *b)
{
	struct ubx_port *p = NULL, *ptmp = NULL;
	struct ubx_config *c = NULL, *ctmp = NULL;

	if (b->meta_data)
		free((char *)b->meta_data);

	DL_FOREACH_SAFE(b->configs, c, ctmp) {
		DL_DELETE(b->configs, c);
		ubx_config_free(c);
	}

	DL_FOREACH_SAFE(b->ports, p, ptmp) {
		DL_DELETE(b->ports, p);
		ubx_port_free(p);
	}

	free(b);
}

static int __ubx_port_add(ubx_block_t *b,
			  const char *name,
			  const char *doc,
			  const uint32_t attrs,
			  const ubx_type_t *in_type,
			  const long in_data_len,
			  const ubx_type_t *out_type,
			  const long out_data_len);

static int __ubx_config_add(ubx_block_t *b,
			    const char *name,
			    const char *doc,
			    const ubx_type_t *type,
			    uint16_t min,
			    uint16_t max,
			    uint32_t attrs);
/**
 * ubx_block_clone - create a copy of an existing block
 *
 * @prot prototype block to clone
 * @name name of new block
 *
 * @return a pointer to the newly allocated block or NULL in case of
 * 	   error. Must be freed with ubx_block_free.
 */
static ubx_block_t *ubx_block_clone(ubx_block_t *prot, const char *name)
{
	int ret;
	ubx_block_t *newb;
	ubx_config_t *csrc = NULL;
	ubx_port_t *psrc = NULL;

	newb = calloc(1, sizeof(ubx_block_t));

	if (newb == NULL) {
		logf_err(prot->nd, "EOUTOFMEM");
		return NULL;
	}

	newb->block_state = BLOCK_STATE_PREINIT;
	strncpy((char*) newb->name, name, UBX_BLOCK_NAME_MAXLEN);
	newb->prototype = prot;

	newb->type = prot->type;
	newb->attrs = prot->attrs;
	newb->meta_data = NULL;

	newb->init = prot->init;
	newb->start = prot->start;
	newb->stop = prot->stop;
	newb->cleanup = prot->cleanup;

	switch (prot->type) {
	case BLOCK_TYPE_COMPUTATION:
		newb->step = prot->step;
		break;
	case BLOCK_TYPE_INTERACTION:
		newb->read = prot->read;
		newb->write = prot->write;
		break;
	}

	DL_FOREACH(prot->configs, csrc) {
		ret = __ubx_config_add(newb,
				       csrc->name,
				       NULL,
				       csrc->type,
				       csrc->min,
				       csrc->max,
				       csrc->attrs | CONFIG_ATTR_CLONED);
		if (ret != 0)
			goto out_free;
	}

	DL_FOREACH(prot->ports, psrc) {
		ret = __ubx_port_add(newb,
				     psrc->name,
				     NULL,
				     psrc->attrs | PORT_ATTR_CLONED,
				     psrc->in_type,
				     psrc->in_data_len,
				     psrc->out_type,
				     psrc->out_data_len);
		if (ret != 0)
			goto out_free;
	}

	return newb;

out_free:
	ubx_block_free(newb);
	return NULL;
}


/**
 * ubx_block_create - create a new block
 *
 * @ni node info
 * @block_type type of block to create
 * @param name
 *
 * @return the newly created block or NULL
 */
ubx_block_t *ubx_block_create(ubx_node_t *nd, const char *type, const char *name)
{
	ubx_block_t *prot, *newb=NULL;

	if (name == NULL) {
		logf_err(nd, "block_create: name is NULL");
		goto out;
	}

	/* find prototype */
	HASH_FIND_STR(nd->blocks, type, prot);

	if (prot == NULL) {
		logf_err(nd, "no such block of type %s", type);
		goto out;
	}

	if (blk_is_instance(prot))
		logf_warn(nd, "cloning from non-prototype block %s", type);

	/* check if name is already used */
	HASH_FIND_STR(nd->blocks, name, newb);

	if (newb != NULL) {
		logf_err(nd, "existing block named %s", name);
		newb = NULL;
		goto out;
	}

	newb = ubx_block_clone(prot, name);

	if (newb == NULL) {
		logf_crit(nd, "failed to create block %s of type %s: out of mem",
			  type, name);
		goto out;
	}

	/* register block */
	if (__block_register(nd, newb) != 0) {
		logf_crit(nd, "failed to register block %s", name);
		ubx_block_free(newb);
		goto out;
	}
 out:
	return newb;
}

/**
 * ubx_block_rm - unregister and free a block.
 *
 * This will unregister a block and free it's data. The block must be
 * in BLOCK_STATE_PREINIT state.
 *
 * TODO: fail if not in state PREINIT.
 *
 * @param ni
 * @param block_type
 * @param name
 *
 * @return 0 if ok, error code otherwise.
 */
int ubx_block_rm(ubx_node_t *nd, const char *name)
{
	int ret = -1;
	ubx_block_t *b;

	b = ubx_block_get(nd, name);

	if (b == NULL) {
		logf_err(nd, "no block %s", name);
		ret = ENOSUCHENT;
		goto out;
	}

	if (b->prototype == NULL) {
		logf_err(nd, "block %s is a prototype", name);
		ret = EINVALID_BLOCK_TYPE;
		goto out;
	}

	if (b->block_state != BLOCK_STATE_PREINIT) {
		logf_err(nd, "block %s not in preinit state", name);
		ret = EWRONG_STATE;
		goto out;
	}

	if (ubx_block_unregister(nd, name) != 0)
		logf_err(nd, "block %s failed to unregister", name);

	ret = 0;
 out:
	return ret;
}

/**
 * array_block_add - add a block to an array
 *
 * grow/shrink the array if necessary.
 *
 * @param arr
 * @param newblock
 *
 * @return < 0 in case of error, 0 otherwise.
 */
static int array_block_add(const ubx_block_t ***arr, const ubx_block_t *newblock)
{
	int ret;
	long newlen; /* new length of array including NULL element */
	const ubx_block_t **tmpb;

	/*
	 * determine newlen
	 * with one element, the array is size two to hold the terminating NULL
	 * element.
	 */
	if (*arr == NULL)
		newlen = 2;
	else
		for (tmpb = *arr, newlen = 2; *tmpb != NULL; tmpb++, newlen++)
			;

	*arr = realloc(*arr, sizeof(ubx_block_t *) * newlen);
	if (*arr == NULL) {
		ret = EOUTOFMEM;
		goto out;
	}

	(*arr)[newlen - 2] = newblock;
	(*arr)[newlen - 1] = NULL;
	ret = 0;
 out:
	return ret;
}

static int array_block_rm(const ubx_block_t ***arr, const ubx_block_t *rmblock)
{
	int ret = -1, cnt, match = -1;
	const ubx_block_t **tmpb;

	for (tmpb = *arr, cnt = 0; *tmpb != NULL; tmpb++, cnt++) {
		if (*tmpb == rmblock)
			match = cnt;
	}

	if (match == -1)
		goto out;

	/* case 1: remove last */
	if (match == cnt - 1)
		(*arr)[match] = NULL;
	else {
		(*arr)[match] = (*arr)[cnt - 1];
		(*arr)[cnt - 1] = NULL;
	}

	ret = 0;
out:
	return ret;
}

/**
 * ubx_port_connect_out - connect a port out channel to an iblock.
 *
 * @param p
 * @param iblock
 *
 * @return  < 0 in case of error, 0 otherwise.
 */
int ubx_port_connect_out(ubx_port_t *p, const ubx_block_t *iblock)
{
	int ret = -1;

	if (port_is_out(p)) {
		ret = array_block_add(&p->out_interaction, iblock);
		if (ret != 0)
			goto out;
	} else {
		ret = EINVALID_PORT_DIR;
		goto out;
	}

	/* all ok */
	ret = 0;
out:
	return ret;
}

/**
 * ubx_port_connect_in - connect a port in channel to an iblock.
 *
 * @param p
 * @param iblock
 *
 * @return < 0 in case of error, 0 otherwise.
 */
int ubx_port_connect_in(ubx_port_t *p, const ubx_block_t *iblock)
{
	int ret;

	if (port_is_in(p)) {
		ret = array_block_add(&p->in_interaction, iblock);
		if (ret != 0)
			goto out;
	} else {
		ret = EINVALID_PORT_DIR;
		goto out;
	}

	/* all ok */
	ret = 0;
out:
	return ret;
}

/**
 * ubx_ports_connect - connect two ports with an iblock
 *
 * @out_port output port
 * @in_port input port
 * @iblock iblock to use for connection
 *
 * @return < 0  in case of error, 0 otherwise.
 */
int ubx_ports_connect(ubx_port_t *out_port, ubx_port_t *in_port, const ubx_block_t *iblock)
{
	int ret;

	if (iblock == NULL) {
		ERR("block NULL");
		return EINVALID_BLOCK_TYPE;
	}

	if (out_port == NULL) {
		logf_err(iblock->nd, "out_port NULL");
		return EINVALID_PORT;
	}

	if (in_port == NULL) {
		logf_err(iblock->nd, "in_port NULL");
		return EINVALID_PORT;
	}

	if (in_port->in_type != out_port->out_type) {
		logf_err(iblock->nd, "ETYPE_MISMATCH: in: %s, out: %s",
			 in_port->in_type->name, in_port->out_type->name);
		return ETYPE_MISMATCH;
	}

	if (in_port->in_data_len != out_port->out_data_len) {
		logf_err(iblock->nd, "EINVALID_PORT_LEN: in: %lu, out: %lu",
			 in_port->in_data_len, in_port->out_data_len);
	}

	if (iblock->type != BLOCK_TYPE_INTERACTION) {
		logf_err(iblock->nd, "block not of type interaction");
		return EINVALID_BLOCK_TYPE;
	}

	ret = ubx_port_connect_out(out_port, iblock);
	if (ret != 0)
		goto out;
	ret = ubx_port_connect_in(in_port, iblock);
	if (ret != 0)
		goto out;

	ret = 0;
out:
	return ret;
}


/**
 * ubx_port_disconnect_out - disconnect port out channel from interaction.
 *
 * @param out_port
 * @param iblock
 *
 * @return < 0 in case of error, 0 otherwise.
 */
int ubx_port_disconnect_out(ubx_port_t *out_port, const ubx_block_t *iblock)
{
	int ret = -1;

	if (port_is_out(out_port)) {
		ret = array_block_rm(&out_port->out_interaction, iblock);
		if (ret != 0)
			goto out;
	} else {
		logf_err(iblock->nd,
			 "port %s is not an out-port",
			 out_port->name);
		ret = EINVALID_PORT_TYPE;
		goto out;
	}
	ret = 0;

out:
	return ret;
}

/**
 * ubx_port_disconnect_in - disconnect port in channel from interaction.
 *
 * @param out_port
 * @param iblock
 *
 * @return < 0 in case of error, 0 otherwise.
 */
int ubx_port_disconnect_in(ubx_port_t *in_port, const ubx_block_t *iblock)
{
	int ret = -1;

	if (port_is_in(in_port)) {
		ret = array_block_rm(&in_port->in_interaction, iblock);
		if (ret != 0)
			goto out;
	} else {
		logf_err(iblock->nd, "port %s is not an in-port", in_port->name);
		ret = EINVALID_PORT_TYPE;
		goto out;
	}
	ret = 0;

out:
	return ret;
}


/**
 * ubx_ports_disconnect - disconnect two ports
 *
 * @out_port output port to disconnect
 * @in_port input port to disconnect
 * @iblock iblock used for connection
 *
 * Note that the iblock itself is not touched
 *
 * @return < 0 in case of error, 0 otherwise.
 */
int ubx_ports_disconnect(ubx_port_t *out_port, ubx_port_t *in_port, const ubx_block_t *iblock)
{
	int ret = -1;

	if (iblock == NULL) {
		ERR("iblock NULL");
		return EINVALID_BLOCK;
	}

	if (out_port == NULL) {
		logf_err(iblock->nd, "out_port NULL");
		return EINVALID_PORT;
	}

	if (in_port == NULL) {
		logf_err(iblock->nd, "in_port NULL");
		return EINVALID_PORT;
	}

	if (iblock->type != BLOCK_TYPE_INTERACTION) {
		logf_err(iblock->nd, "block not of type interaction");
		return EINVALID_BLOCK_TYPE;
	}

	ret = ubx_port_disconnect_out(out_port, iblock);
	if (ret != 0)
		goto out;
	ret = ubx_port_disconnect_in(in_port, iblock);
	if (ret != 0)
		goto out;

	ret = 0;
out:
	return ret;
}


/*
 * Configuration
 */

/**
 * ubx_config_get - retrieve a configuration type by name.
 *
 * @param b
 * @param name
 *
 * @return ubx_config_t pointer or NULL if not found.
 */
ubx_config_t *ubx_config_get(const ubx_block_t *b, const char *name)
{
	ubx_config_t *c = NULL;

	if (b == NULL) {
		ERR("port_get: block is NULL");
		return NULL;
	}

	if (name == NULL) {
		ubx_err(b, "config_get: config name is NULL");
		return NULL;
	}

	DL_FOREACH(b->configs, c) {
		if (strncmp(c->name, name, UBX_CONFIG_NAME_MAXLEN) == 0)
			return c;
	}

	return NULL;
}

/**
 * ubx_config_get_data - return the data associated with a configuration value.
 *
 * @param b
 * @param name
 *
 * @return ubx_data_t pointer or NULL
 */
ubx_data_t *ubx_config_get_data(const ubx_block_t *b, const char *name)
{
	ubx_config_t *conf;
	ubx_data_t *data = NULL;

	conf = ubx_config_get(b, name);

	if (conf == NULL)
		goto out;

	data = conf->value;

 out:
	return data;
}

/**
 * ubx_config_get_data_ptr - get pointer to and length of configuration data
 *
 * @param b ubx_block
 * @param name name of the requested configuration value
 * @param ptr if successfull, this ptr will be set to the ubx_data_t.
 *
 * @return length of configuration (>0), 0 if unconfigured, < 0 in case of error
 */
long ubx_config_get_data_ptr(const ubx_block_t *b, const char *name, void **ptr)
{
	ubx_data_t *d;

	d = ubx_config_get_data(b, name);

	if (d == NULL)
		return EINVALID_CONFIG;

	/* only modify ptr if configured */
	if (d->len > 0)
		*ptr = d->data;

	return d->len;
}

/**
 * ubx_config_data_len - return array length of a configuration
 *
 * @param b
 * @param cfg_name name of configuration
 *
 * @return array length of the given configuration value
 */
long ubx_config_data_len(const ubx_block_t *b, const char *cfg_name)
{
	ubx_data_t *d = ubx_config_get_data(b, cfg_name);

	if (!d)
		return -1;

	return d->len;
}


/**
 * ubx_config_add - internal version with already resolved type
 *
 * @param b
 * @param name
 * @param doc
 * @param type_name
 *
 * @return 0 if Ok, !=0 otherwise.
 */
static int __ubx_config_add(ubx_block_t *b,
			    const char *name,
			    const char *doc,
			    const ubx_type_t *type,
			    uint16_t min,
			    uint16_t max,
			    uint32_t attrs)
{
	int ret;
	ubx_config_t *cnew;

	if (ubx_config_get(b, name)) {
		ubx_err(b, "config_add: %s already exists", name);
		ret = EENTEXISTS;
		goto out;
	}

	cnew = calloc(1, sizeof(struct ubx_config));

	if (cnew == NULL) {
		ubx_err(b, "EOUTOFMEM");
		ret = EOUTOFMEM;
		goto out;
	}

	if (strlen(name) > UBX_CONFIG_NAME_MAXLEN) {
		ubx_err(b, "config_add: name %s too long (max: %u)",
			name, UBX_CONFIG_NAME_MAXLEN);
		ret = EINVALID_CONFIG;
		goto out_free;
	}

	strncpy((char*) cnew->name, name, UBX_CONFIG_NAME_MAXLEN);

	cnew->min = min;
	cnew->max = (max == 0 && min > 0) ? CONFIG_LEN_MAX : max;
	cnew->attrs = attrs;

	if (cnew->max < cnew->min) {
		ubx_err(b, "config_add: invalid min/max [%u/%u] for %s",
			cnew->min, cnew->max, name);
		return EINVALID_CONFIG;;
	}

	if (doc) {
		cnew->doc = strdup(doc);
		if (cnew->doc == NULL) {
			ubx_err(b, "EOUTOFMEM: failed to alloc config %s docstr", name);
			ret = EOUTOFMEM;
			goto out_free;
		};
	}

	cnew->type = type;
	cnew->value = __ubx_data_alloc(type, 0);
	cnew->block = b;

	DL_APPEND(b->configs, cnew);
	return 0;

out_free:
	free(cnew);
out:
	return ret;
}

/**
 * ubx_config_add2 - add a new config to a block.
 *
 * this version allows setting min/max and attrs too.
 *
 * @b
 * @name
 * @doc
 * @type_name
 * @min
 * @max
 * @attrs
 *
 * @return 0 if Ok, !=0 otherwise.
 */
int ubx_config_add2(ubx_block_t *b,
		    const char *name,
		    const char *doc,
		    const char *type_name,
		    uint16_t min,
		    uint16_t max,
		    uint32_t attrs)
{
	ubx_type_t *type;

	if (b == NULL)
		return EINVALID_BLOCK;

	if (name == NULL) {
		ubx_err(b, "config_add: name is NULL");
		return EINVALID_ARG;
	}

	type = ubx_type_get(b->nd, type_name);

	if (type == NULL) {
		ubx_err(b, "config_add: %s: unknown type %s", name, type_name);
		return EINVALID_TYPE;
	}

	return __ubx_config_add(b, name, doc, type, min, max, attrs);
}

/**
 * ubx_config_add - add a new config to a block.
 *
 * @b
 * @name
 * @doc
 * @type_name
 *
 * @return 0 if Ok, !=0 otherwise.
 */
int ubx_config_add(ubx_block_t *b,
		   const char *name,
		   const char *doc,
		   const char *type_name)
{
	return ubx_config_add2(b, name, doc, type_name, 0, 0, 0);
}


/**
 * ubx_config_rm - remove a config from a block and free it.
 *
 * @b block from which to remove config
 * @name name of config to remove
 *
 * @return < 0 in case of error, 0 otherwise.
 */
int ubx_config_rm(ubx_block_t *b, const char *name)
{
	ubx_config_t *c;

	if (b == NULL || name == NULL)
		return EINVALID_ARG;

	c = ubx_config_get(b, name);

	if (c == NULL) {
		ubx_err(b, "ubx_config_rm: no config %s found", name);
		return ENOSUCHENT;
	}

	DL_DELETE(b->configs, c);

	ubx_config_free(c);

	return 0;
}


/**
 * check_minmax - validate min/max attributes
 *
 * possible min,max settings:
 *   - 0,0:              no constraints
 *   - 0,1:              zero or one
 *   - 0,CONFIG_LEN_MAX: zero to many
 *   - N,unset:          zero to many (for N>0)
 *   - N,N:              exactly N
 *
 * @param b block
 * @param checklate 1 if this is a late check (in start), 0 otherwise
 *
 * @return 0 if successful, EINVALID_CONFIG_LEN otherwise
 */
static int check_minmax(const ubx_block_t *b, const int checklate)
{
	int ret = 0;
	struct ubx_config *c = NULL;

	DL_FOREACH(b->configs, c) {
		if (c->min == 0 && c->max == 0)
			continue;

		if (((c->attrs & CONFIG_ATTR_CHECKLATE) != 0 && !checklate) ||
		    ((c->attrs & CONFIG_ATTR_CHECKLATE) == 0 && checklate)) {
			ubx_debug(b, "skipping checking of %s", c->name);
			continue;
		}

		/* distinguish for nicer error msgs */
		if (c->value->len == 0 && c->min > 0) {
			ubx_err(b, "EINVALID_CONFIG_LEN: mandatory config %s unset", c->name);
			ret = EINVALID_CONFIG_LEN;
		} else if (c->value->len < c->min || c->value->len > c->max) {
			ubx_err(b, "EINVALID_CONFIG_LEN: config %s len %lu not in min/max [%u/%u]",
				c->name, c->value->len, c->min, c->max);
			ret = EINVALID_CONFIG_LEN;
		}
		ubx_debug(b, "%s %s OK", __func__, c->name);
	}

	return ret;
}

/**
 * __ubx_port_add - internal version with resolved types
 *
 * @b
 * @name
 * @doc
 * @in_type
 * @in_data_len
 * @out_type
 * @out_data_len
 *
 * @return < 0 in case of error, 0 otherwise.
 */
static int __ubx_port_add(ubx_block_t *b,
			  const char *name,
			  const char *doc,
			  const uint32_t attrs,
			  const ubx_type_t *in_type,
			  const long in_data_len,
			  const ubx_type_t *out_type,
			  const long out_data_len)
{
	int ret;
	ubx_port_t *pnew;

	if (ubx_port_get(b, name)) {
		ubx_err(b, "port_add: %s already exists", name);
		return EENTEXISTS;
	}

	pnew = calloc(1, sizeof(struct ubx_port));

	if (pnew == NULL) {
		ubx_err(b, "EOUTOFMEM");
		ret = EOUTOFMEM;
		goto out;
	}

	if (strlen(name) > UBX_PORT_NAME_MAXLEN) {
		ubx_err(b, "port_add: name %s too long (max: %u)",
			name, UBX_PORT_NAME_MAXLEN);
		ret = EINVALID_PORT;
		goto out_free;
	}

	strncpy((char*) pnew->name, name, UBX_PORT_NAME_MAXLEN);

	if (doc) {
		pnew->doc = strdup(doc);
		if (pnew->doc == NULL) {
			ubx_err(b, "EOUTOFMEM: failed to alloc port %s docstr", name);
			ret = EOUTOFMEM;
			goto out_free;
		}
	}

	pnew->attrs = attrs;
	pnew->in_type = in_type;
	pnew->out_type = out_type;
	pnew->in_data_len = (in_data_len == 0) ? 1 : in_data_len;
	pnew->out_data_len = (out_data_len == 0) ? 1 : out_data_len;

	pnew->block = b;

	DL_APPEND(b->ports, pnew);

	return 0;

out_free:
	free(pnew);
out:
	return ret;
}

/**
 * ubx_port_add - a port to a block instance and resolve types.
 *
 * @param b
 * @param name
 * @param doc
 * @param in_type_name
 * @param in_data_len
 * @param out_type_name
 * @param out_data_len
 *
 * @return < 0 in case of error, 0 otherwise.
 */
int ubx_port_add(ubx_block_t *b,
		 const char *name,
		 const char *doc,
		 const uint32_t attrs,
		 const char *in_type_name,
		 const long in_data_len,
		 const char *out_type_name,
		 const long out_data_len)
{
	ubx_type_t *in_type = NULL, *out_type = NULL;

	if (b == NULL)
		return EINVALID_BLOCK;

	if (in_type_name) {
		in_type = ubx_type_get(b->nd, in_type_name);
		if (in_type == NULL) {
			ubx_err(b, "failed to resolve in_type %s", in_type_name);
			return EINVALID_TYPE;
		}
	}

	if (out_type_name) {
		out_type = ubx_type_get(b->nd, out_type_name);
		if (out_type == NULL) {
			ubx_err(b, "failed to resolve out_type %s", out_type_name);
			return EINVALID_TYPE;
		}
	}
	return __ubx_port_add(b, name, doc, attrs, in_type, in_data_len, out_type, out_data_len);
}

/**
 * ubx_outport_add - add an output port
 *
 * @param b
 * @param name
 * @param out_type_name
 * @param out_data_len
 * @return < 0 in case of error, 0 otherwise
 */
int ubx_outport_add(ubx_block_t *b, const char *name, const char *doc, uint32_t attrs,
		    const char *out_type_name, long out_data_len)
{
	return ubx_port_add(b, name, doc, attrs,
			    NULL, 0, out_type_name, out_data_len);
}

/**
 * ubx_inport_resize - change the array length of the port
 *
 * This will fail unless the block is in state BLOCK_STATE_PREINIT
 * @param p port to resize
 * @param len new length
 * @return 0 if	OK, <0 otherwise
 */
int ubx_inport_resize(struct ubx_port *p, long len)
{
	if (p->block->block_state != BLOCK_STATE_PREINIT) {
		ubx_err(p->block, "inport_resize %s: can't resize block in state %s",
			p->name, block_state_tostr(p->block->block_state));
		return -1;
	}
	p->in_data_len = len;
	return 0;
}

/**
 * ubx_outport_resize - change the array length of the port
 *
 * This will fail unless the block is in state BLOCK_STATE_PREINIT
 * @param p port to resize
 * @param len new length
 * @return 0 if	OK, <0 otherwise
 */
int ubx_outport_resize(struct ubx_port *p, long len)
{
	if (p->block->block_state != BLOCK_STATE_PREINIT) {
		ubx_err(p->block, "outport_resize %s: can't resize block in state %s",
			p->name, block_state_tostr(p->block->block_state));
		return -1;
	}
	p->out_data_len = len;
	return 0;
}

/**
 * ubx_inport_add - add an input-port
 *
 * @param b
 * @param name
 * @param in_type_name
 * @param in_data_len
 * @return < 0 in case of error, 0 otherwise
 */
int ubx_inport_add(ubx_block_t *b, const char *name, const char *doc, uint32_t attrs,
		   const char *in_type_name, long in_data_len)
{
	return ubx_port_add(b, name, doc, attrs, in_type_name, in_data_len, NULL, 0);
}

/**
 * ubx_port_rm - remove a port from a block.
 *
 * @param b
 * @param name
 *
 * @return
 */
int ubx_port_rm(ubx_block_t *b, const char *name)
{
	struct ubx_port *p;

	if (b == NULL || name == NULL)
		return EINVALID_ARG;

	p = ubx_port_get(b, name);

	if (p == NULL) {
		ubx_err(b, "ubx_port_rm: no port %s found", name);
		return ENOSUCHENT;
	}

	DL_DELETE(b->ports, p);

	ubx_port_free(p);

	return 0;
}


/**
 * ubx_port_get - retrieve a block port by name
 *
 * @param b
 * @param name
 *
 * @return port pointer or NULL
 */
ubx_port_t *ubx_port_get(const ubx_block_t *b, const char *name)
{
	ubx_port_t *p = NULL;

	if (b == NULL) {
		ERR("port_get: block is NULL");
		return NULL;
	}

	if (name == NULL) {
		ubx_err(b, "port_get: port name is NULL");
		return NULL;
	}

	DL_FOREACH(b->ports, p) {
		if (strncmp(p->name, name, UBX_PORT_NAME_MAXLEN) == 0)
			return p;
	}

	return NULL;
}


/**
 * ubx_block_init - initalize a function block.
 *
 * @param ni
 * @param b
 *
 * @return 0 if state was changed, non-zero otherwise.
 */
int ubx_block_init(ubx_block_t *b)
{
	int ret;

	if (b == NULL) {
		ret = EINVALID_BLOCK;
		goto out;
	}

	ubx_debug(b, __func__);

	/* check and use loglevel config */
	if (ubx_config_get(b, "loglevel") == NULL ||
	    cfg_getptr_int(b, "loglevel", &b->loglevel) <= 0)
		b->loglevel = NULL;
	else
		ubx_debug(b, "found loglevel config");

	if (b->block_state != BLOCK_STATE_PREINIT) {
		ubx_err(b, "init: not in state preinit (but %s)",
			block_state_tostr(b->block_state));
		ret = EWRONG_STATE;
		goto out;
	}

	ret = check_minmax(b, 0);

	if (ret < 0)
		goto out;

	if (b->init == NULL)
		goto out_ok;

	ret = b->init(b);
	if (ret != 0) {
		ubx_err(b, "init failed");
		goto out;
	}

 out_ok:
	b->block_state = BLOCK_STATE_INACTIVE;
	ret = 0;

 out:
	return ret;
}

/**
 * ubx_block_start - start a function block.
 *
 * @param ni
 * @param b
 *
 * @return 0 if state was changed, non-zero otherwise.
 */
int ubx_block_start(ubx_block_t *b)
{
	int ret;

	if (b == NULL) {
		ret = EINVALID_BLOCK;
		goto out;
	}

	ubx_debug(b, __func__);

	if (b->block_state != BLOCK_STATE_INACTIVE) {
		ubx_err(b, "start: not in state inactive (but %s)",
			block_state_tostr(b->block_state));
		ret = EWRONG_STATE;
		goto out;
	}

	ret = check_minmax(b, 1);

	if (ret < 0)
		goto out;

	b->block_state = BLOCK_STATE_ACTIVE;

	if (b->start == NULL)
		goto out_ok;

	ret = b->start(b);

	if (ret != 0) {
		b->block_state = BLOCK_STATE_INACTIVE;
		ubx_err(b, "start failed");
		goto out;
	}

 out_ok:
	ret = 0;

 out:
	return ret;
}

/**
 * ubx_block_stop - stop a function block
 *
 * @param ni
 * @param b
 *
 * @return
 */
int ubx_block_stop(ubx_block_t *b)
{
	int ret;

	if (b == NULL) {
		ret = EINVALID_BLOCK;
		goto out;
	}

	ubx_debug(b, __func__);

	if (b->block_state != BLOCK_STATE_ACTIVE) {
		ubx_err(b, "stop: not in state active (but %s)",
			block_state_tostr(b->block_state));
		ret = EWRONG_STATE;
		goto out;
	}

	if (b->stop == NULL)
		goto out_ok;

	b->stop(b);

 out_ok:
	b->block_state = BLOCK_STATE_INACTIVE;
	ret = 0;

 out:
	return ret;
}

/**
 * ubx_block_cleanup - bring function block back to preinit state.
 *
 * @param ni
 * @param b
 *
 * @return 0 if state was changed, non-zero otherwise.
 */
int ubx_block_cleanup(ubx_block_t *b)
{
	int ret;

	if (b == NULL) {
		ret = EINVALID_BLOCK;
		goto out;
	}

	ubx_debug(b, __func__);

	if (b->block_state != BLOCK_STATE_INACTIVE) {
		ubx_err(b, "cleanup: not in state inactive (but %s)",
			block_state_tostr(b->block_state));
		ret = EWRONG_STATE;
		goto out;
	}

	if (b->cleanup == NULL)
		goto out_ok;

	b->cleanup(b);

 out_ok:
	b->block_state = BLOCK_STATE_PREINIT;
	ret = 0;

 out:
	return ret;
}

/**
 * Step a cblock
 *
 * @param b
 *
 * @return 0 if OK, else -1
 */
int ubx_cblock_step(ubx_block_t *b)
{
	int ret;

	if (b == NULL) {
		ret = EINVALID_BLOCK;
		goto out;
	}

	if (b->type != BLOCK_TYPE_COMPUTATION) {
		ubx_err(b, "cblock_step: invalid block type %u", b->type);
		ret = EINVALID_BLOCK_TYPE;
		goto out;
	}

	if (b->block_state != BLOCK_STATE_ACTIVE) {
		ubx_err(b, "cblock_step: block not active");
		ret = EWRONG_STATE;
		goto out;
	}

	if (b->step == NULL)
		goto out_ok;

	b->step(b);
	b->stat_num_steps++;

out_ok:
	ret = 0;

out:
	return ret;
}


/**
 * @brief
 *
 * @param port port from which to read
 * @param data ubx_data_t to store result
 *
 * @return status value
 */
long __port_read(const ubx_port_t *port, ubx_data_t *data)
{
	int ret = 0;
	ubx_block_t **iaptr;

	if (port == NULL) {
		ERR("port is NULL");
		ret = EINVALID_PORT;
		goto out;
	}

	if (!data) {
		ret = EINVALID_ARG;
		goto out;
	}

	if (data->len <= 0) {
		ret = EINVALID_ARG;
		goto out;
	}

	if (!port_is_in(port)) {
		ret = EINVALID_PORT_DIR;
		goto out;
	};

	if (port->in_type != data->type) {
		ret = ETYPE_MISMATCH;
		ubx_err(port->block, "port_read %s: type mismatch: data: %s, port: %s",
			port->name,
			get_typename(data),
			port->in_type->name);
		goto out;
	}

	/* port completely unconnected? */
	if (port->in_interaction == NULL)
		goto out;

	for (iaptr = (ubx_block_t**)port->in_interaction; *iaptr != NULL; iaptr++) {
		if ((*iaptr)->block_state == BLOCK_STATE_ACTIVE) {
			ret = (*iaptr)->read(*iaptr, data);
			if (ret > 0) {
				(*iaptr)->stat_num_reads++;
				goto out;
			}
		}
	}

 out:
	return ret;
}

/**
 * __port_write - write a sample to a port
 * @param port
 * @param data
 *
 * This function will check if the type matches.
 */
void __port_write(const ubx_port_t *port, const ubx_data_t *data)
{
	/* int i; */
	const char *tp;
	ubx_block_t **iaptr;

	if (port == NULL) {
		ERR("port is NULL");
		goto out;
	}

	if (!port_is_out(port)) {
		ubx_err(port->block, "not an OUT-port");
		goto out;
	};

	if (port->out_type != data->type) {
		tp = get_typename(data);
		ubx_err(port->block,
			"port_write %s: type mismatch: data: %s, port: %s",
			port->name, tp, port->out_type->name);
		goto out;
	}

	/* port completely unconnected? */
	if (port->out_interaction == NULL)
		goto out;

	/* pump it out */
	for (iaptr = (ubx_block_t**)port->out_interaction; *iaptr != NULL; iaptr++) {
		if ((*iaptr)->block_state == BLOCK_STATE_ACTIVE) {
			(*iaptr)->write(*iaptr, data);
			(*iaptr)->stat_num_writes++;
		}
	}

 out:
	return;
}

/**
 * ubx_version - return ubx version
 *
 * @return version string major.minor.patchlevel
 */
const char *ubx_version(void)
{
	return VERSION;
}

