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

#include <config.h>

#include "ubx.h"
#ifdef TIMESRC_TSC
#include <math.h>
#endif

/*
 * Internal helper functions
 */

/* core logging helpers */
#define CORE_LOG_SRC			"ubxcore"

#define log_emerg(ni, fmt, ...)		ubx_log(UBX_LOGLEVEL_EMERG,  ni, CORE_LOG_SRC, fmt, ##__VA_ARGS__)
#define log_alert(ni, fmt, ...)		ubx_log(UBX_LOGLEVEL_ALERT,  ni, CORE_LOG_SRC, fmt, ##__VA_ARGS__)
#define log_crit(ni,  fmt, ...)		ubx_log(UBX_LOGLEVEL_CRIT,   ni, CORE_LOG_SRC, fmt, ##__VA_ARGS__)
#define log_err(ni, fmt, ...)		ubx_log(UBX_LOGLEVEL_ERR,    ni, CORE_LOG_SRC, fmt, ##__VA_ARGS__)
#define log_warn(ni, fmt, ...)		ubx_log(UBX_LOGLEVEL_WARN,   ni, CORE_LOG_SRC, fmt, ##__VA_ARGS__)
#define log_notice(ni, fmt, ...)	ubx_log(UBX_LOGLEVEL_NOTICE, ni, CORE_LOG_SRC, fmt, ##__VA_ARGS__)
#define log_info(ni, fmt, ...)		ubx_log(UBX_LOGLEVEL_INFO,   ni, CORE_LOG_SRC, fmt, ##__VA_ARGS__)

#define logf_emerg(ni, fmt, ...)	ubx_log(UBX_LOGLEVEL_EMERG,  ni, __func__, fmt, ##__VA_ARGS__)
#define logf_alert(ni, fmt, ...)	ubx_log(UBX_LOGLEVEL_ALERT,  ni, __func__, fmt, ##__VA_ARGS__)
#define logf_crit(ni,  fmt, ...)	ubx_log(UBX_LOGLEVEL_CRIT,   ni, __func__, fmt, ##__VA_ARGS__)
#define logf_err(ni, fmt, ...)		ubx_log(UBX_LOGLEVEL_ERR,    ni, __func__, fmt, ##__VA_ARGS__)
#define logf_warn(ni, fmt, ...)		ubx_log(UBX_LOGLEVEL_WARN,   ni, __func__, fmt, ##__VA_ARGS__)
#define logf_notice(ni, fmt, ...)	ubx_log(UBX_LOGLEVEL_NOTICE, ni, __func__, fmt, ##__VA_ARGS__)
#define logf_info(ni, fmt, ...)		ubx_log(UBX_LOGLEVEL_INFO,   ni, __func__, fmt, ##__VA_ARGS__)

#ifdef UBX_DEBUG
# define log_debug(ni, fmt, ...)	ubx_log(UBX_LOGLEVEL_DEBUG,  ni, CORE_LOG_SRC, fmt, ##__VA_ARGS__)
# define logf_debug(ni, fmt, ...)	ubx_log(UBX_LOGLEVEL_DEBUG,  ni, __func__, fmt, ##__VA_ARGS__)
#else
# define log_debug(ni, fmt, ...)	do {} while (0)
# define logf_debug(ni, fmt, ...)	do {} while (0)
#endif


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
int ubx_module_load(ubx_node_info_t *ni, const char *lib)
{
	int ret = -1;
	char *err;
	ubx_module_t *mod;

	HASH_FIND_STR(ni->modules, lib, mod);

	if (mod != NULL) {
		logf_err(ni, "module %s already loaded", lib);
		goto out;
	}

	/* allocate data */
	mod = calloc(sizeof(ubx_module_t), 1);
	if (mod == NULL) {
		logf_err(ni, "failed to alloc module data");
		goto out;
	}

	mod->id = strdup(lib);
	if (mod->id == NULL) {
		logf_err(ni, "cloning mod name failed");
		goto out_err_free_mod;
	}

	mod->handle = dlopen(lib, RTLD_NOW);
	if (mod->handle == NULL) {
		ubx_log(UBX_LOGLEVEL_ERR, ni, "dlopen", "%s", dlerror());
		goto out_err_free_id;
	}

	dlerror();

	mod->init = dlsym(mod->handle, "__ubx_initialize_module");
	err = dlerror();
	if (err != NULL)  {
		logf_err(ni, "no module_init for mod %s found: %s", lib, err);
		goto out_err_close;
	}

	dlerror();

	mod->cleanup = dlsym(mod->handle, "__ubx_cleanup_module");
	err = dlerror();
	if (err != NULL)  {
		logf_err(ni, "no module_cleanup found for %s: %s", lib, err);
		goto out_err_close;
	}

	dlerror();

	mod->spdx_license_id = dlsym(mod->handle, "__ubx_module_license_spdx");
	err = dlerror();
	if (err != NULL)
		logf_warn(ni, "missing UBX_MODULE_LICENSE_SPDX in module %s", lib);

	/* execute module init */
	if (mod->init(ni) != 0) {
		logf_err(ni, "module_init of %s failed", lib);
		goto out_err_close;
	}

	/* register with node */
	HASH_ADD_KEYPTR(hh, ni->modules, mod->id, strlen(mod->id), mod);

	logf_debug(ni, "loaded %s", lib);
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

ubx_module_t *ubx_module_get(ubx_node_info_t *ni, const char *lib)
{
	ubx_module_t *mod = NULL;

	HASH_FIND_STR(ni->modules, lib, mod);
	return mod;
}

/**
 * ubx_module_unload - unload a module from a node.
 *
 * @param ni node_info
 * @param lib name of module library to unload
 */
void ubx_module_unload(ubx_node_info_t *ni, const char *lib)
{
	ubx_module_t *mod;

	HASH_FIND_STR(ni->modules, lib, mod);

	if (mod == NULL) {
		logf_err(ni, "unknown module %s", lib);
		goto out;
	}

	HASH_DEL(ni->modules, mod);

	mod->cleanup(ni);

	dlclose(mod->handle);
	free((char *)mod->id);
	free(mod);
 out:
	return;
}

/**
 * initalize node_info
 *
 * @param ni
 *
 * @return 0 if ok, -1 otherwise.
 */
int ubx_node_init(ubx_node_info_t *ni, const char *name, uint32_t attrs)
{
	int ret = -1;

	ni->loglevel = (ni->loglevel == 0) ?
		UBX_LOGLEVEL_DEFAULT : ni->loglevel;

	if (ubx_log_init(ni)) {
		fprintf(stderr, "Error: failed to initalize logging.");
		goto out;
	}

	if (name == NULL) {
		logf_err(ni, "node name is NULL");
		goto out;
	}

	logf_notice(ni, "node_init: %s, loglevel: %u", name, ni->loglevel);

	ni->name = strdup(name);
	if (ni->name == NULL) {
		logf_err(ni, "strdup failed: %m");
		goto out;
	}

	if (attrs & ND_DUMPABLE) {
		if (prctl(PR_SET_DUMPABLE, 1, 0, 0, 0) != 0) {
			logf_err(ni, "enabling core dumps PR_SET_DUMPABLE): %m");
			goto out;
		}
		logf_info(ni, "core dumps enabled (PR_SET_DUMPABLE)");
	}

	if (attrs & ND_MLOCK_ALL) {
		if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
			logf_err(ni, "mlockall failed CUR|FUT: %m");
			goto out;
		};
		logf_info(ni, "mlockall CUR|FUT succeeded");
	}

	ni->attrs = attrs;
	ni->blocks = NULL;
	ni->types = NULL;
	ni->modules = NULL;
	ni->cur_seqid = 0;

	ret = 0;
 out:
	return ret;
}

/**
 * ubx_node_cleanup - cleanup a node
 *
 * This stops, cleanups and rm's all blocks and unloads all
 * modules. The node is empty but still valid afterwards.
 *
 * @param ni
 */
void ubx_node_cleanup(ubx_node_info_t *ni)
{
	int cnt;
	ubx_module_t *m = NULL, *mtmp = NULL;
	ubx_block_t *b = NULL, *btmp = NULL;

	logf_debug(ni, "cleaning up node %s", ni->name);

	/* stop all blocks */
	HASH_ITER(hh, ni->blocks, b, btmp) {
		if (b->block_state == BLOCK_STATE_ACTIVE) {
			logf_debug(ni, "stopping block %s", b->name);
			if (ubx_block_stop(b) != 0)
				logf_err(ni, "failed to stop block %s", b->name);
		}
	}

	/* cleanup all blocks */
	HASH_ITER(hh, ni->blocks, b, btmp) {
		if (b->block_state == BLOCK_STATE_INACTIVE) {
			logf_debug(ni, "cleaning up block %s", b->name);
			if (ubx_block_cleanup(b) != 0)
				logf_err(ni, "failed to cleanup block %s", b->name);
		}
	}

	/* rm all non prototype blocks */
	HASH_ITER(hh, ni->blocks, b, btmp) {
		if (b->block_state == BLOCK_STATE_PREINIT && b->prototype != NULL) {
			logf_debug(ni, "removing block %s", b->name);
			if (ubx_block_rm(ni, b->name) != 0)
				logf_err(ni, "ubx_block_rm failed for %s", b->name);
		}
	}

	/* unload all modules. */
	HASH_ITER(hh, ni->modules, m, mtmp) {
		logf_debug(ni, "unloading module %s", m->id);
		ubx_module_unload(ni, m->id);
	}

	cnt = ubx_num_types(ni);
	if (cnt > 0)
		logf_warn(ni, "%d types after cleanup", cnt);
	cnt = ubx_num_modules(ni);
	if (cnt > 0)
		logf_warn(ni, "%d modules after cleanup", cnt);
	cnt = ubx_num_blocks(ni);
	if (cnt > 0)
		logf_warn(ni, "%d blocks after cleanup", cnt);

	ni->cur_seqid = 0;
}

/**
 * ubx_node_rm - cleanup an destroy a node
 *
 * calls ubx_node_cleanup and frees node member memory. Must not be
 * used afterwards.
 *
 * @param ni
 */
void ubx_node_rm(ubx_node_info_t *ni)
{
	logf_info(ni, "removing node %s", ni->name);
	ubx_node_cleanup(ni);
	ubx_log_cleanup(ni);
	free((char *) ni->name);
	ni->name = NULL;
}

/**
 * ubx_block_register - register a block with the given node_info.
 *
 * @param ni
 * @param block
 *
 * @return 0 if Ok, < 0 otherwise.
 */
int ubx_block_register(ubx_node_info_t *ni, ubx_block_t *block)
{
	int ret = -1;
	ubx_config_t *c;
	ubx_block_t *tmpc;

	/*
	 * don't allow instances to be registered into more
	 * than one node_info
	 */
	if (block->prototype != NULL && block->ni != NULL) {
		logf_err(ni, "block %s already registered", block->name);
		goto out;
	}

	if (block->type != BLOCK_TYPE_COMPUTATION &&
	    block->type != BLOCK_TYPE_INTERACTION) {
		logf_err(ni, "invalid block type %d", block->type);
		goto out;
	}

	/* consistency checks */
	for (unsigned int i = 0; i < get_num_configs(block); i++) {
		c = &block->configs[i];

		/*
		 * if min is set and max is unset, then max defaults
		 * to CONFIG_LEN_MAX
		 */
		if (c->max == 0 && c->min > 0)
			c->max = CONFIG_LEN_MAX;

		if (c->max < c->min) {
			logf_err(ni, "block %s config %s: invalid min/max [%u/%u]",
				 block->name, c->name, c->min, c->max);
			goto out;
		}
	}

	HASH_FIND_STR(ni->blocks, block->name, tmpc);

	if (tmpc != NULL) {
		log_err(ni, "block %s already registered", block->name);
		goto out;
	}

	block->ni = ni;

	/* resolve types */
	ret = ubx_resolve_types(block);
	if (ret != 0)
		goto out;

	HASH_ADD_KEYPTR(hh, ni->blocks, block->name, strlen(block->name), block);
	logf_debug(ni, "registered %s", block->name);
	ret = 0;
 out:
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
ubx_block_t *ubx_block_get(ubx_node_info_t *ni, const char *name)
{
	ubx_block_t *tmpc = NULL;

	if (name == NULL)
		goto out;

	HASH_FIND_STR(ni->blocks, name, tmpc);
out:
	return tmpc;
}

/**
 * ubx_block_unregister - unregister a block.
 *
 * @param ni
 * @param type
 * @param name
 *
 * @return the unregistered block or NULL in case of failure.
 */
ubx_block_t *ubx_block_unregister(ubx_node_info_t *ni, const char *name)
{
	ubx_block_t *tmpc;

	HASH_FIND_STR(ni->blocks, name, tmpc);

	if (tmpc == NULL) {
		logf_err(ni, "block %s not registered", name);
		goto out;
	}

	HASH_DEL(ni->blocks, tmpc);
 out:
	return tmpc;
}


/**
 * ubx_type_register - register a type with a node.
 *
 * @param ni
 * @param type
 *
 * @return
 */
int ubx_type_register(ubx_node_info_t *ni, ubx_type_t *type)
{
	int ret;
	ubx_type_t *tmp;

	if (type == NULL) {
		logf_err(ni, "type is NULL");
		ret = EINVALID_TYPE;
		goto out;
	}

	if (type->name == NULL) {
		logf_err(ni, "type name is NULL");
		ret = EINVALID_TYPE;
		goto out;
	}

	/* check that its not already registered */
	HASH_FIND_STR(ni->types, type->name, tmp);

	if (tmp != NULL) {
		/* check if types are the same, if yes no error */
		logf_warn(ni, "%s already registered.", tmp->name);
		ret = EALREADY_REGISTERED;
		goto out;
	}

	type->seqid = ni->cur_seqid++;
	type->ni = ni;

	/* compute md5 fingerprint for type */
	md5((const unsigned char *)type->name, strlen(type->name), type->hash);

	HASH_ADD_KEYPTR(hh, ni->types, type->name, strlen(type->name), type);

	logf_debug(ni, "registered type %s: seqid %lu, ptr %p",
		   type->name, type->seqid, type);
	ret = 0;
 out:
	return ret;
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
ubx_type_t *ubx_type_unregister(ubx_node_info_t *ni, const char *name)
{
	ubx_type_t *ret;

	HASH_FIND_STR(ni->types, name, ret);

	if (ret == NULL) {
		logf_err(ni, "type %s not registered", name);
		goto out;
	}

	HASH_DEL(ni->types, ret);

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
ubx_type_t *ubx_type_get(ubx_node_info_t *ni, const char *name)
{
	ubx_type_t *type = NULL;

	if (name == NULL) {
		logf_debug(ni, "name argument is NULL");
		goto out;
	}

	HASH_FIND_STR(ni->types, name, type);
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
ubx_type_t *ubx_type_get_by_hash(ubx_node_info_t *ni, const uint8_t *hash)
{
	ubx_type_t *type = NULL, *tmptype = NULL;

	HASH_ITER(hh, ni->types, type, tmptype) {
		if (strncmp((char *)type->hash,
			    (char *)hash, TYPE_HASH_LEN) == 0)
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
ubx_type_t *ubx_type_get_by_hashstr(ubx_node_info_t *ni, const char *hashstr)
{
	int len;
	char tmp[3];
	uint8_t hash[TYPE_HASH_LEN+1];

	len = strlen(hashstr);

	if (len != TYPE_HASHSTR_LEN) {
		logf_err(ni, "invalid length of hashstr %u", len);
		return NULL;
	}

	/* convert from string to binary array */
	for (int i = 0; i < TYPE_HASH_LEN; i++) {
		tmp[0] = hashstr[i * 2];
		tmp[1] = hashstr[i * 2 + 1];
		tmp[2] = '\0';
		hash[i] = (uint8_t) strtoul(tmp, NULL, 16);
	}
	hash[TYPE_HASH_LEN] = '\0';

	return ubx_type_get_by_hash(ni, hash);
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
	for (int i = 0; i < TYPE_HASH_LEN; i++)
		sprintf(buf + i * 2, "%02x", t->hash[i]);
	buf[TYPE_HASH_LEN * 2] = '\0';
}


/**
 * ubx_resolve_types - resolve string type references to real type object.
 *
 * For each port and config, let the type pointers point to the
 * correct type identified by the char* name. Error if failure.
 *
 * @param ni
 * @param b
 *
 * @return 0 if all types are resolved, -1 if not.
 */
int ubx_resolve_types(ubx_block_t *b)
{
	int ret = EINVALID_TYPE;
	ubx_type_t *typ;
	ubx_port_t *port_ptr;
	ubx_config_t *config_ptr;
	ubx_node_info_t *ni = b->ni;

	/* for each port locate type and resolve */
	if (b->ports) {
		for (port_ptr = b->ports; port_ptr->name != NULL; port_ptr++) {
			/* in-type */
			if (port_ptr->in_type_name) {
				typ = ubx_type_get(ni, port_ptr->in_type_name);
				if (typ == NULL) {
					ubx_err(b, "in-port %s: unknown type %s",
						port_ptr->name,
						port_ptr->in_type_name);
					goto out;
				}
				port_ptr->in_type = typ;
			}

			/* out-type */
			if (port_ptr->out_type_name) {
				typ = ubx_type_get(ni, port_ptr->out_type_name);
				if (typ == NULL) {
					ubx_err(b, "out-port %s: unknown type %s",
						port_ptr->name,
						port_ptr->out_type_name);
					goto out;
				}
				port_ptr->out_type = typ;
			}
		}
	}

	/* configs */
	if (b->configs != NULL) {
		for (config_ptr = b->configs; config_ptr->name != NULL; config_ptr++) {
			typ = ubx_type_get(ni, config_ptr->type_name);
			if (typ == NULL)  {
				ubx_err(b, "config %s: unknown type %s",
					config_ptr->name,
					config_ptr->type_name);
				goto out;
			}
			config_ptr->type = typ;
		}
	}

	/* all ok */
	ret = 0;
 out:
	return ret;
}

/**
 * Allocate a ubx_data_t of the given type and array length.
 *
 * This type should be free'd using the ubx_data_free function.
 *
 * @param ubx_type_t of the new type
 * @param array_len
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
		goto out_nomem;

	d->type = typ;
	d->len = array_len;

	d->data = calloc(array_len, typ->size);
	if (d->data == NULL)
		goto out_nomem;

	/* all ok */
	goto out;

 out_nomem:
	if (d)
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
ubx_data_t *ubx_data_alloc(ubx_node_info_t *ni,
			   const char *typname,
			   long array_len)
{
	ubx_type_t *t = NULL;
	ubx_data_t *d = NULL;

	if (ni == NULL) {
		ERR("node_info NULL");
		goto out;
	}

	t = ubx_type_get(ni, typname);
	if (t == NULL) {
		logf_err(ni, "unknown type %s", typname);
		goto out;
	}

	d = __ubx_data_alloc(t, array_len);

	if (!d)
		logf_err(ni, "failed to alloc type %s[%ld]", typname, array_len);
 out:
	return d;
}

int ubx_data_resize(ubx_data_t *d, long newlen)
{
	int ret = -1;
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

int ubx_num_blocks(ubx_node_info_t *ni)
{
	return HASH_COUNT(ni->blocks);
}

int ubx_num_types(ubx_node_info_t *ni)
{
	return HASH_COUNT(ni->types);
}

int ubx_num_modules(ubx_node_info_t *ni)
{
	return HASH_COUNT(ni->modules);
}

/**
 * ubx_port_free_data - free additional memory used by port.
 *
 * @param p port pointer
 */
void ubx_port_free_data(ubx_port_t *p)
{
	if (p->out_type_name)
		free((char *)p->out_type_name);
	if (p->in_type_name)
		free((char *)p->in_type_name);

	if (p->in_interaction)
		free((struct ubx_block_t *)p->in_interaction);
	if (p->out_interaction)
		free((struct ubx_block_t *)p->out_interaction);

	if (p->doc)
		free((char *)p->doc);
	if (p->name)
		free((char *)p->name);

	memset(p, 0x0, sizeof(ubx_port_t));
}

/**
 * ubx_clone_port_data - clone the additional port data
 *
 * @param p pointer to (allocated) target port.
 * @param name name of port
 * @param doc port documentation string
 *
 * @param in_type_name string name of in-port data
 * @param in_type_len max array size of transported data
 *
 * @param out_type_name string name of out-port data
 * @param out_type_len max array size of transported data
 *
 * @param state state of port.
 *
 * @return less than zero if an error occured, 0 otherwise.
 *
 * This function allocates memory.
 */
int ubx_clone_port_data(ubx_port_t *p, const char *name, const char *doc,
			ubx_type_t *in_type, long in_data_len,
			ubx_type_t *out_type, long out_data_len, uint32_t state)
{
	int ret = EOUTOFMEM;

	assert(name != NULL);

	memset(p, 0x0, sizeof(ubx_port_t));

	p->name = strdup(name);
	if (p->name == NULL)
		goto out;

	if (doc) {
		p->doc = strdup(doc);
		if (p->doc == NULL)
			goto out_err_free;
	}

	if (in_type != NULL) {
		p->in_type_name = strdup(in_type->name);
		if (p->in_type_name == NULL)
			goto out_err_free;
		p->in_type = in_type;
		p->attrs |= PORT_DIR_IN;
	}

	if (out_type != NULL) {
		p->out_type_name = strdup(out_type->name);
		if (p->out_type_name == NULL)
			goto out_err_free;
		p->out_type = out_type;
		p->attrs |= PORT_DIR_OUT;
	}

	p->in_data_len = (in_data_len == 0) ? 1 : in_data_len;
	p->out_data_len = (out_data_len == 0) ? 1 : out_data_len;

	p->state = state;

	/* all ok */
	ret = 0;
	goto out;

 out_err_free:
	ubx_port_free_data(p);
 out:
	return ret;
}

/**
 * ubx_config_free_data - free a config's extra memory
 *
 * @param c config whose data to free
 */
static void ubx_config_free_data(ubx_config_t *c)
{
	if (c->name)
		free((char *)c->name);
	if (c->doc)
		free((char *)c->doc);
	if (c->type_name)
		free((char *)c->type_name);
	if (c->value)
		ubx_data_free(c->value);
	memset(c, 0x0, sizeof(ubx_config_t));
}

/**
 * ubx_clone_conf_data - clone the additional config data
 *
 * @param cnew pointer to (allocated) ubx_config
 * @param name name of configuration
 * @param type ubx_type_t of configuration value
 * @param len array size of data
 *
 * @return less than zero if an error occured, 0 otherwise.
 *
 * This function allocates memory.
 */
static int ubx_clone_config_data(ubx_config_t *cnew,
				 const char *name,
				 const char *doc,
				 const ubx_type_t *type,
				 const long len)
{
	memset(cnew, 0x0, sizeof(ubx_config_t));

	cnew->name = strdup(name);
	if (cnew->name == NULL)
		goto out_err;

	if (doc) {
		cnew->doc = strdup(doc);
		if (cnew->doc == NULL)
			goto out_err;
	}

	cnew->type_name = strdup(type->name);
	if (cnew->type_name == NULL)
		goto out_err;

	cnew->value = __ubx_data_alloc(type, len);

	return 0; /* all ok */

 out_err:
	ubx_config_free_data(cnew);
	return -EOUTOFMEM;
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
	/* configs */
	if (b->configs != NULL) {
		ubx_config_t *config_ptr;

		for (config_ptr = b->configs; config_ptr->name != NULL; config_ptr++)
			ubx_config_free_data(config_ptr);
		free(b->configs);
	}

	/* ports */
	if (b->ports != NULL) {
		ubx_port_t *port_ptr;

		for (port_ptr = b->ports; port_ptr->name != NULL; port_ptr++)
			ubx_port_free_data(port_ptr);
		free(b->ports);
	}

	if (b->prototype)
		free(b->prototype);
	if (b->meta_data)
		free((char *)b->meta_data);
	if (b->name)
		free((char *)b->name);
	if (b)
		free(b);
}


/**
 * ubx_block_clone - create a copy of an existing block from an existing one.
 *
 * @param prot prototype block which to clone
 * @param name name of new block
 *
 * @return a pointer to the newly allocated block. Must be freed with
 */
static ubx_block_t *ubx_block_clone(ubx_block_t *prot, const char *name)
{
	int i;
	ubx_block_t *newb;
	ubx_port_t *srcport, *tgtport;
	ubx_config_t *srcconf, *tgtconf;

	newb = calloc(1, sizeof(ubx_block_t));
	if (newb == NULL)
		goto out_free;

	newb->block_state = BLOCK_STATE_PREINIT;

	/* copy attributes of prototype */
	newb->type = prot->type;
	newb->attrs = prot->attrs;

	newb->name = strdup(name);
	if (newb->name == NULL)
		goto out_free;
	newb->meta_data = strdup(prot->meta_data);
	if (newb->meta_data == NULL)
		goto out_free;
	newb->prototype = strdup(prot->name);
	if (newb->prototype == NULL)
		goto out_free;

	/* copy configuration */
	if (prot->configs) {
		i = get_num_configs(prot);

		newb->configs = calloc(i + 1, sizeof(ubx_config_t));
		if (newb->configs == NULL)
			goto out_free;

		for (srcconf = prot->configs, tgtconf = newb->configs;
		    srcconf->name != NULL; srcconf++, tgtconf++) {
			if (ubx_clone_config_data(tgtconf,
						  srcconf->name,
						  srcconf->doc,
						  srcconf->type,
						  srcconf->data_len) != 0)
				goto out_free;
			tgtconf->min = srcconf->min;
			tgtconf->max = srcconf->max;
			tgtconf->block = newb;
		}
	}


	/* do we have ports? */
	if (prot->ports) {
		i = get_num_ports(prot);

		newb->ports = calloc(i + 1, sizeof(ubx_port_t));
		if (newb->ports == NULL)
			goto out_free;

		for (srcport = prot->ports, tgtport = newb->ports;
		     srcport->name != NULL;
		     srcport++, tgtport++) {
			if (ubx_clone_port_data(tgtport, srcport->name, srcport->doc,
						srcport->in_type, srcport->in_data_len,
						srcport->out_type, srcport->out_data_len,
						srcport->state) != 0)
				goto out_free;
			tgtport->block = newb;
		}
	}

	/* ops */
	newb->init = prot->init;
	newb->start = prot->start;
	newb->stop = prot->stop;
	newb->cleanup = prot->cleanup;

	/* copy special ops */
	switch (prot->type) {
	case BLOCK_TYPE_COMPUTATION:
		newb->step = prot->step;
		newb->stat_num_steps = 0;
		break;
	case BLOCK_TYPE_INTERACTION:
		newb->read = prot->read;
		newb->write = prot->write;
		newb->stat_num_reads = 0;
		newb->stat_num_writes = 0;
		break;
	}

	/* all ok */
	return newb;

 out_free:
	ubx_block_free(newb);
	return NULL;
}


/**
 * @brief instantiate new block of type type with name
 *
 * @param ni
 * @param block_type
 * @param name
 *
 * @return the newly created block or NULL
 */
ubx_block_t *ubx_block_create(ubx_node_info_t *ni, const char *type, const char *name)
{
	ubx_block_t *prot, *newb;

	newb = NULL;

	if (name == NULL) {
		logf_err(ni, "block_create: name is NULL");
		goto out;
	}

	/* find prototype */
	HASH_FIND_STR(ni->blocks, type, prot);

	if (prot == NULL) {
		logf_err(ni, "no such block %s", type);
		goto out;
	}

	/* check if name is already used */
	HASH_FIND_STR(ni->blocks, name, newb);

	if (newb != NULL) {
		logf_err(ni, "existing block named %s", name);
		newb = NULL;
		goto out;
	}

	newb = ubx_block_clone(prot, name);
	if (newb == NULL) {
		logf_crit(ni, "failed to create block %s of type %s: out of mem",
			  type, name);
		goto out;
	}

	/* register block */
	if (ubx_block_register(ni, newb) != 0) {
		logf_crit(ni, "failed to register block %s", name);
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
int ubx_block_rm(ubx_node_info_t *ni, const char *name)
{
	int ret = -1;
	ubx_block_t *b;

	b = ubx_block_get(ni, name);

	if (b == NULL) {
		logf_err(ni, "no block %s", name);
		ret = ENOSUCHENT;
		goto out;
	}

	if (b->prototype == NULL) {
		logf_err(ni, "block %s is a prototype", name);
		ret = EINVALID_BLOCK_TYPE;
		goto out;
	}

	if (b->block_state != BLOCK_STATE_PREINIT) {
		logf_err(ni, "block %s not in preinit state", name);
		ret = EWRONG_STATE;
		goto out;
	}

	if (ubx_block_unregister(ni, name) == NULL)
		logf_err(ni, "block %s failed to unregister", name);

	ubx_block_free(b);
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
static int array_block_add(ubx_block_t ***arr, ubx_block_t *newblock)
{
	int ret;
	long newlen; /* new length of array including NULL element */
	ubx_block_t **tmpb;

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

static int array_block_rm(ubx_block_t ***arr, ubx_block_t *rmblock)
{
	int ret = -1, cnt, match = -1;
	ubx_block_t **tmpb;

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
int ubx_port_connect_out(ubx_port_t *p, ubx_block_t *iblock)
{
	int ret = -1;

	if (p->attrs & PORT_DIR_OUT) {
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

int ubx_port_connect_in(ubx_port_t *p, ubx_block_t *iblock)
{
	int ret;

	if (p->attrs & PORT_DIR_IN) {
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
 * ubx_ports_connect_uni
 *   - connect a port out- to a port in-channel using an interaction iblock.
 *
 * @param out_port
 * @param in_port
 * @param iblock
 *
 * @return < 0  in case of error, 0 otherwise.
 */
int ubx_ports_connect_uni(ubx_port_t *out_port, ubx_port_t *in_port, ubx_block_t *iblock)
{
	int ret;

	if (iblock == NULL) {
		ERR("block NULL");
		return EINVALID_BLOCK_TYPE;
	}

	if (out_port == NULL) {
		logf_err(iblock->ni, "out_port NULL");
		return EINVALID_PORT;
	}

	if (in_port == NULL) {
		logf_err(iblock->ni, "in_port NULL");
		return EINVALID_PORT;
	}

	if (iblock->type != BLOCK_TYPE_INTERACTION) {
		logf_err(iblock->ni, "block not of type interaction");
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
int ubx_port_disconnect_out(ubx_port_t *out_port, ubx_block_t *iblock)
{
	int ret = -1;

	if (out_port->attrs & PORT_DIR_OUT) {
		ret = array_block_rm(&out_port->out_interaction, iblock);
		if (ret != 0)
			goto out;
	} else {
		logf_err(iblock->ni,
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
int ubx_port_disconnect_in(ubx_port_t *in_port, ubx_block_t *iblock)
{
	int ret = -1;

	if (in_port->attrs & PORT_DIR_IN) {
		ret = array_block_rm(&in_port->in_interaction, iblock);
		if (ret != 0)
			goto out;
	} else {
		logf_err(iblock->ni, "port %s is not an in-port", in_port->name);
		ret = EINVALID_PORT_TYPE;
		goto out;
	}
	ret = 0;

out:
	return ret;
}


/**
 * ubx_ports_disconnect_uni - disconnect two ports.
 *
 * @param out_port
 * @param in_port
 * @param iblock
 *
 * @return < 0 in case of error, 0 otherwise.
 */
int ubx_ports_disconnect_uni(ubx_port_t *out_port, ubx_port_t *in_port, ubx_block_t *iblock)
{
	int ret = -1;

	if (iblock == NULL) {
		ERR("iblock NULL");
		return EINVALID_BLOCK;
	}

	if (out_port == NULL) {
		logf_err(iblock->ni, "out_port NULL");
		return EINVALID_PORT;
	}

	if (in_port == NULL) {
		logf_err(iblock->ni, "in_port NULL");
		return EINVALID_PORT;
	}

	if (iblock->type != BLOCK_TYPE_INTERACTION) {
		logf_err(iblock->ni, "block not of type interaction");
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
 * get_num_configs - get the number of configs of a block
 *
 * @param b block
 *
 * @return number of configs of block b
 */
unsigned int get_num_configs(const ubx_block_t *b)
{
	unsigned int n;

	if (b->configs == NULL)
		n = 0;
	else
		for (n = 0; b->configs[n].name != NULL; n++)
			;

	return n;
}

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
	ubx_config_t *conf = NULL;

	if (b == NULL)
		goto out;

	if (b->configs == NULL || name == NULL)
		goto out;

	for (conf = b->configs; conf->name != NULL; conf++)
		if (strcmp(conf->name, name) == 0)
			goto out;
	conf = NULL;

 out:
	return conf;
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
	long ret = -1;

	d = ubx_config_get_data(b, name);
	if (d == NULL)
		goto out;

	*ptr = d->data;
	ret = d->len;

 out:
	return ret;
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
 * ubx_config_add - add a new ubx_config value to an existing block.
 *
 * @param b
 * @param name
 * @param meta
 * @param type_name
 * @param len
 *
 * @return 0 if Ok, !=0 otherwise.
 */
int ubx_config_add(ubx_block_t *b,
		   const char *name,
		   const char *meta,
		   const char *type_name,
		   const long len)
{
	ubx_type_t *typ;
	ubx_config_t *carr;
	int i, ret;

	if (b == NULL) {
		ret = EINVALID_BLOCK;
		goto out;
	}

	typ = ubx_type_get(b->ni, type_name);
	if (typ == NULL) {
		ubx_err(b, "unkown type %s", type_name);
		ret = EINVALID_TYPE;
		goto out;
	}

	if (ubx_config_get(b, name)) {
		ubx_err(b, "config_add: %s already exists", name);
		ret = EENTEXISTS;
		goto out;
	}

	i = get_num_configs(b);

	carr = realloc(b->configs, (i + 2) * sizeof(ubx_config_t));

	if (carr == NULL) {
		ubx_err(b, "out of mem, config not added");
		ret = EOUTOFMEM;
		goto out;
	}

	b->configs = carr;

	ret = ubx_clone_config_data(&b->configs[i], name, meta, typ, len);

	if (ret != 0) {
		ubx_err(b, "cloning config data failed");
		goto out;
	}

	b->configs[i].block = b;

	memset(&b->configs[i + 1], 0x0, sizeof(ubx_config_t));

	ret = 0;
 out:
	return ret;
}

/**
 * ubx_config_rm - remove a config from a block and free it.
 *
 * @param b
 * @param name
 *
 * @return < 0 in case of error, 0 otherwise.
 */
int ubx_config_rm(ubx_block_t *b, const char *name)
{
	int ret = -1, i, num_configs;

	if (!b) {
		ret = EINVALID_BLOCK;
		goto out;
	}

	if (b->prototype == NULL) {
		ubx_err(b, "modifying prototype block not allowed");
		ret = EINVALID_BLOCK_TYPE;
		goto out;
	}

	if (b->configs == NULL) {
		ubx_err(b, "no config %s found", name);
		ret = ENOSUCHENT;
		goto out;
	}

	num_configs = get_num_configs(b);

	for (i = 0; i < num_configs; i++)
		if (strcmp(b->configs[i].name, name) == 0)
			break;

	if (i >= num_configs) {
		ubx_err(b, "no config %s found", name);
		goto out;
	}

	ubx_config_free_data(&b->configs[i]);

	if (i < num_configs - 1) {
		b->configs[i] = b->configs[num_configs - 1];
		memset(&b->configs[num_configs - 1], 0x0, sizeof(ubx_config_t));
	}

	ret = 0;
 out:
	return ret;
}

/**
 * validate_configs - validate array length of configs
 *
 * possible min,max settings:
 *   - 0,0:              no constraints
 *   - 0,1:              zero or one
 *   - 0,CONFIG_LEN_MAX: zero to many
 *   - N,unset:          zero to many (for N>0)
 *   - N,N:              exactly N
 *
 *   basic sanity checks are in ubx_block_register
 *
 * @param b block
 * @param checklate 1 if this is a late check (in start), 0 otherwise
 *
 * @return 0 if successful, EINVALID_CONFIG_LEN otherwise
 */
static int validate_configs(const ubx_block_t *b, const int checklate)
{
	int ret = 0;

	for (unsigned int i = 0; i < get_num_configs(b); i++) {

		ubx_config_t *c = &b->configs[i];

		/* no constraints defined */
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


/*
 * Ports
 */

/**
 * get_num_ports - get the number of ports of a block
 *
 * @param b block
 *
 * @return number of ports of block b
 */
unsigned int get_num_ports(ubx_block_t *b)
{
	unsigned int n;

	if (b->ports == NULL)
		n = 0;
	else
		for (n = 0; b->ports[n].name != NULL; n++)
			; /* find number of ports */

	return n;
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
 * @param state
 *
 * @return < 0 in case of error, 0 otherwise.
 */
int ubx_port_add(ubx_block_t *b, const char *name, const char *doc,
	     const char *in_type_name, long in_data_len,
	     const char *out_type_name, long out_data_len, uint32_t state)
{
	int i, ret;
	ubx_port_t *parr;
	ubx_type_t *in_type = NULL, *out_type = NULL;

	if (b == NULL) {
		ret = EINVALID_BLOCK;
		goto out;
	}

	if (b->prototype == NULL) {
		ubx_err(b, "modifying prototype block not allowed");
		ret = EINVALID_BLOCK_TYPE;
		goto out;
	}

	if (in_type_name) {
		in_type = ubx_type_get(b->ni, in_type_name);
		if (in_type == NULL) {
			ubx_err(b, "failed to resolve in_type %s", in_type_name);
			ret = EINVALID_TYPE;
			goto out;
		}
	}

	if (out_type_name) {
		out_type = ubx_type_get(b->ni, out_type_name);
		if (out_type == NULL) {
			ubx_err(b, "failed to resolve out_type %s", out_type_name);
			ret = EINVALID_TYPE;
			goto out;
		}
	}

	if (ubx_port_get(b, name)) {
		ubx_err(b, "port_add: %s already exists", name);
		ret = EENTEXISTS;
		goto out;
	}

	i = get_num_ports(b);

	parr = realloc(b->ports, (i + 2) * sizeof(ubx_port_t));

	if (parr == NULL) {
		ubx_err(b, "out of mem, port not added");
		ret = EOUTOFMEM;
		goto out;
	}

	b->ports = parr;

	/* setup port */
	ret = ubx_clone_port_data(&b->ports[i], name, doc,
				in_type, in_data_len,
				out_type, out_data_len, state);

	b->ports[i].block = b;

	if (ret) {
		ubx_err(b, "cloning port data failed");
		memset(&b->ports[i], 0x0, sizeof(ubx_port_t));
		/* nothing else to cleanup, really */
	}

	/* set dummy stopper to NULL */
	memset(&b->ports[i + 1], 0x0, sizeof(ubx_port_t));

 out:
	return ret;
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
int ubx_outport_add(ubx_block_t *b, const char *name, const char *doc,
		    const char *out_type_name, long out_data_len)
{
	return ubx_port_add(b, name, doc, NULL, 0, out_type_name, out_data_len, 1);
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
int ubx_inport_add(ubx_block_t *b, const char *name, const char *doc,
		   const char *in_type_name, long in_data_len)
{
	return ubx_port_add(b, name, doc, in_type_name, in_data_len, NULL, 0, 1);
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
	int ret = -1, i, num_ports;

	if (!b) {
		ret = EINVALID_BLOCK;
		goto out;
	}

	if (b->prototype == NULL) {
		ubx_err(b, "modifying prototype block not allowed");
		goto out;
	}

	if (b->ports == NULL) {
		ubx_err(b, "no port %s found", name);
		goto out;
	}

	num_ports = get_num_ports(b);

	for (i = 0; i < num_ports; i++)
		if (strcmp(b->ports[i].name, name) == 0)
			break;

	if (i >= num_ports) {
		ubx_err(b, "no port %s found", name);
		goto out;
	}

	ubx_port_free_data(&b->ports[i]);

	/*
	 * if the removed port is the last in list, there is nothing
	 * to do. if not, copy the last port into the removed one [i]
	 * and zero last one
	 */
	if (i < num_ports - 1) {
		b->ports[i] = b->ports[num_ports - 1];
		memset(&b->ports[num_ports - 1], 0x0, sizeof(ubx_port_t));
	}

	ret = 0;
 out:
	return ret;
}


/**
 * ubx_port_get - retrieve a block port by name
 *
 * @param b
 * @param name
 *
 * @return port pointer or NULL
 */
ubx_port_t *ubx_port_get(ubx_block_t *b, const char *name)
{
	ubx_port_t *port_ptr = NULL;

	if (b == NULL) {
		ERR("block is NULL");
		goto out;
	}

	if (name == NULL) {
		ubx_err(b, "port_get: name is NULL");
		goto out;
	}

	if (b->ports == NULL)
		goto out;

	for (port_ptr = b->ports; port_ptr->name != NULL; port_ptr++)
		if (strcmp(port_ptr->name, name) == 0)
			goto out;

	/* no port found */
	port_ptr = NULL;
out:
	return port_ptr;
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

	ret = validate_configs(b, 0);
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

	ret = validate_configs(b, 1);
	if (ret < 0)
		goto out;

	if (b->start == NULL)
		goto out_ok;

	ret = b->start(b);
	if (ret != 0) {
		ubx_err(b, "start failed");
		goto out;
	}

 out_ok:
	b->block_state = BLOCK_STATE_ACTIVE;
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
		ubx_err(b, "invalid block type %u", b->type);
		ret = EINVALID_BLOCK_TYPE;
		goto out;
	}

	if (b->block_state != BLOCK_STATE_ACTIVE) {
		ubx_err(b, "block not active");
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

	if ((port->attrs & PORT_DIR_IN) == 0) {
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

	for (iaptr = port->in_interaction; *iaptr != NULL; iaptr++) {
		if ((*iaptr)->block_state == BLOCK_STATE_ACTIVE) {
			ret = (*iaptr)->read(*iaptr, data);
			if (ret > 0) {
				((ubx_port_t *)port)->stat_reads++;
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

	if ((port->attrs & PORT_DIR_OUT) == 0) {
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
	for (iaptr = port->out_interaction; *iaptr != NULL; iaptr++) {
		if ((*iaptr)->block_state == BLOCK_STATE_ACTIVE) {
			(*iaptr)->write(*iaptr, data);
			(*iaptr)->stat_num_writes++;
		}
	}

	((ubx_port_t *)port)->stat_writes++;

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

