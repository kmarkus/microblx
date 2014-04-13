/*
 * microblx: embedded, real-time safe, reflective function blocks.
 * Copyright (C) 2013,2014 Markus Klotzbuecher <markus.klotzbuecher@mech.kuleuven.be>
 *
 * microblx is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 or (at your option)
 * any later version.
 *
 * microblx is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with eCos; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * As a special exception, if other files instantiate templates or use
 * macros or inline functions from this file, or you compile this file
 * and link it with other works to produce a work based on this file,
 * this file does not by itself cause the resulting work to be covered
 * by the GNU General Public License. However the source code for this
 * file must still be made available in accordance with section (3) of
 * the GNU General Public License.
 *
 * This exception does not invalidate any other reasons why a work
 * based on this file might be covered by the GNU General Public
 * License.
*/

/* #define DEBUG 1 */

#include "ubx.h"

/*
 * Internal helper functions
 */

/* for pretty printing */
const char *block_states[] = {	"preinit", "inactive", "active" };

/**
 * Check type in ports
 * 
 * @param ni ubx block node pointer
 * @param required ubx type required 
 * @param tcheck_str check string
 * @param port string to port name
 * @param isrd is read?
 * 
 * @return int to result: 0 is success, -1 is failure 
 */
int checktype(ubx_node_info_t* ni, ubx_type_t *required, const char *tcheck_str, const char *portname, int isrd)
{
#ifdef CONFIG_TYPECHECK_EXTRA
        ubx_type_t *tcheck = ubx_type_get(ni, tcheck_str);

        assert(ni!=NULL);
        assert(required!=NULL);
        assert(tcheck_str!=NULL);
        assert(portname!=NULL);

        if (required != tcheck) {
                ERR("port %s type error during %s: is '%s' but should be '%s'",
                    portname, (isrd==1) ? "read" : "write", tcheck_str, required->name);
                return -1;
        }
#endif
        return 0;
}

/**
 * Convert block state to string.
 *
 * @param state
 *
 * @return const char pointer to string name.
 */
const char* block_state_tostr(int state)
{
	if(state>=sizeof(block_states))
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
const char* get_typename(ubx_data_t *data)
{
	if(data && data->type)
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
int ubx_module_load(ubx_node_info_t* ni, const char *lib)
{
	int ret = -1;
	char* err;
	ubx_module_t* mod;

	HASH_FIND_STR(ni->modules, lib, mod);

	if(mod != NULL) {
		ERR("module '%s' already loaded in node %s.", lib, ni->name);
		goto out;
	};

	/* allocate data */
	if((mod = calloc(sizeof(ubx_module_t), 1)) == NULL) {
		ERR("failed to alloc module data");
		goto out;
	}

	if((mod->id = strdup(lib))==NULL) {
		ERR("failed to clone module name");
		goto out_err_free_mod;
	}

	if((mod->handle = dlopen(lib, RTLD_NOW)) == NULL) {
		fprintf(stderr, "%s\n", dlerror());
		goto out_err_free_id;
	}

	dlerror();

	mod->init = dlsym(mod->handle, "__ubx_initialize_module");
	if ((err = dlerror()) != NULL)  {
		ERR("failed to lookup __ubx_initialize_module for module %s: %s", lib, err);
		goto out_err_close;
	}

	dlerror();

	mod->cleanup = dlsym(mod->handle, "__ubx_cleanup_module");
	if ((err = dlerror()) != NULL)  {
		ERR("failed to lookup __ubx_cleanup_module for module %s: %s", lib, err);
		goto out_err_close;
	}

	dlerror();

	mod->spdx_license_id = dlsym(mod->handle, "__ubx_module_license_spdx");
	if ((err = dlerror()) != NULL)  {
		MSG("Warning: missing license in module %s. Please define UBX_MODULE_LICENSE_SPDX", lib);
	}

	/* execute module init */
	if(mod->init(ni) != 0)
		goto out_err_close;

	/* register with node */
	HASH_ADD_KEYPTR(hh, ni->modules, mod->id, strlen(mod->id), mod);

	ret=0;
	goto out;


 out_err_close:
	dlclose(mod->handle);
 out_err_free_id:
	free((char*) mod->id);
 out_err_free_mod:
	free(mod);
 out:
	return ret;
}


/**
 * ubx_module_unload - unload a module from a node.
 *
 * @param ni node_info
 * @param lib name of module library to unload
 */
void ubx_module_unload(ubx_node_info_t* ni, const char *lib)
{
	ubx_module_t *mod;

	HASH_FIND_STR(ni->modules, lib, mod);

	if(mod==NULL) {
		ERR("module '%s' not registered.", lib);
		goto out;
	}

	HASH_DEL(ni->modules, mod);

	mod->cleanup(ni);

	dlclose(mod->handle);
	free((char*) mod->id);
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
int ubx_node_init(ubx_node_info_t* ni, const char *name)
{
	int ret=-1;

#ifdef CONFIG_DUMPABLE
	if(prctl(PR_SET_DUMPABLE, 1, 0, 0, 0)!=0)
		ERR2(errno, "failed to set PR_SET_DUMPABLE");
	else
		DBG("Process option PR_SET_DUMPABLE set");
#endif

#ifdef CONFIG_MLOCK_ALL
	if(mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
		ERR2(errno, " ");
		goto out_err;
	};
#endif

	if(name==NULL) {
		ERR("name is NULL");
		goto out;
	}

	if((ni->name=strdup(name))==NULL) {
		ERR("strdup failed");
		goto out;
	}

	ni->blocks=NULL;
	ni->types=NULL;
	ni->modules=NULL;
	ni->cur_seqid=0;
	ret=0;
 out:
	return ret;
}

void ubx_node_cleanup(ubx_node_info_t* ni)
{
	int cnt;
	ubx_module_t *m, *mtmp;
	ubx_block_t *b, *btmp;

	/* stop all blocks */
	HASH_ITER(hh, ni->blocks, b, btmp) {
		if (b->block_state == BLOCK_STATE_ACTIVE) {
			DBG("stopping block %s", b->name);
			if(ubx_block_stop(b)!=0) ERR("%s: failed to stop block %s", ni->name, b->name);
		}
	}

	/* cleanup all blocks */
	HASH_ITER(hh, ni->blocks, b, btmp) {
		if (b->block_state == BLOCK_STATE_INACTIVE) {
			DBG("cleaning up block %s", b->name);
			if(ubx_block_cleanup(b)!=0) ERR("%s: failed to cleanup block %s", ni->name, b->name);
		}
	}

	/* rm all non prototype blocks */
	HASH_ITER(hh, ni->blocks, b, btmp) {
		if (b->block_state == BLOCK_STATE_PREINIT && b->prototype!=NULL) {
			DBG("removing block %s", b->name);
			if(ubx_block_rm(ni, b->name)!=0) ERR("%s: failed to rm block %s", ni->name, b->name);
		}
	}

	/* unload all modules. */
	HASH_ITER(hh, ni->modules, m, mtmp) {
		DBG("unloading module %s", m->id);
		ubx_module_unload(ni, m->id);
	}

	if((cnt = ubx_num_types(ni)) > 0) ERR("node %s: %d types after cleanup", ni->name, cnt);
	if((cnt = ubx_num_modules(ni)) > 0) ERR("node %s: %d modules after cleanup", ni->name, cnt);
	if((cnt = ubx_num_blocks(ni)) > 0) ERR("node %s: %d blocks after cleanup", ni->name, cnt);

	free((char*) ni->name);
	ni->name=NULL;
}


/**
 * ubx_block_register - register a block with the given node_info.
 *
 * @param ni
 * @param block
 *
 * @return 0 if Ok, < 0 otherwise.
 */
int ubx_block_register(ubx_node_info_t *ni, ubx_block_t* block)
{
	int ret = -1;
	ubx_block_t *tmpc;

	/* don't allow instances to be registered into more than one node_info */
	if(block->prototype != NULL && block->ni != NULL) {
		ERR("block %s already registered with node %s", block->name, block->ni->name);
		goto out;
	}

	if(block->type!=BLOCK_TYPE_COMPUTATION &&
	   block->type!=BLOCK_TYPE_INTERACTION) {
		ERR("invalid block type %d", block->type);
		goto out;
	}

	/* TODO: consistency check */

	HASH_FIND_STR(ni->blocks, block->name, tmpc);

	if(tmpc!=NULL) {
		ERR("block with name '%s' already registered.", block->name);
		goto out;
	};

	block->ni=ni;

	/* resolve types */
	if((ret=ubx_resolve_types(block)) !=0)
		goto out;

	HASH_ADD_KEYPTR(hh, ni->blocks, block->name, strlen(block->name), block);

	ret=0;
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
ubx_block_t* ubx_block_get(ubx_node_info_t *ni, const char *name)
{
	ubx_block_t *tmpc=NULL;
	HASH_FIND_STR(ni->blocks, name, tmpc);
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
ubx_block_t* ubx_block_unregister(ubx_node_info_t* ni, const char* name)
{
	ubx_block_t *tmpc=NULL;

	HASH_FIND_STR(ni->blocks, name, tmpc);

	if(tmpc==NULL) {
		ERR("block '%s' not registered.", name);
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
int ubx_type_register(ubx_node_info_t* ni, ubx_type_t* type)
{
	/* DBG(" node=%s, type registered=%s", ni->name, type->name); */

	int ret = -1;
	ubx_type_ref_t* typref;

	if(type==NULL) {
		ERR("given type is NULL");
		goto out;
	}

	/* TODO consistency checkm type->name must exists,... */
	HASH_FIND_STR(ni->types, type->name, typref);

	if(typref!=NULL) {
		/* check if types are the same, if yes no error */
		ERR("type '%s' already registered.", type->name);
		goto out;
	};

	if((typref=malloc(sizeof(ubx_type_ref_t)))==NULL) {
		ERR("failed to alloc struct type_ref");
		goto out;
	}

	typref->type_ptr=type;
	typref->seqid=ni->cur_seqid++;

	/* compute md5 fingerprint for type */
	md5((const unsigned char*) type->name, strlen(type->name), type->hash);

	HASH_ADD_KEYPTR(hh, ni->types, type->name, strlen(type->name), typref);
	ret = 0;
 out:
	return ret;
}

/**
 * ubx_type_unregister - unregister type with node
 *
 * TODO: use count handling, only succeed unloading when not used!
 *
 * @param ni
 * @param name
 *
 * @return
 */
ubx_type_t* ubx_type_unregister(ubx_node_info_t* ni, const char* name)
{
	ubx_type_t* ret = NULL;
	ubx_type_ref_t* typref = NULL;

	HASH_FIND_STR(ni->types, name, typref);

	if(typref==NULL) {
		ERR("no type '%s' registered.", name);
		goto out;
	};

	HASH_DEL(ni->types, typref);
	ret=typref->type_ptr;
	free(typref);
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
ubx_type_t* ubx_type_get(ubx_node_info_t* ni, const char* name)
{
	ubx_type_ref_t* typref=NULL;

	HASH_FIND_STR(ni->types, name, typref);

	if(typref)
		return typref->type_ptr;

	return NULL;
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
int ubx_resolve_types(ubx_block_t* b)
{
	int ret = -1;
	ubx_type_t* typ;
	ubx_port_t* port_ptr;
	ubx_config_t *config_ptr;

	ubx_node_info_t* ni = b->ni;

	/* for each port locate type and resolve */
	if(b->ports) {
		for(port_ptr=b->ports; port_ptr->name!=NULL; port_ptr++) {
			/* in-type */
			if(port_ptr->in_type_name) {
				if((typ = ubx_type_get(ni, port_ptr->in_type_name)) == NULL) {
					ERR("failed to resolve type '%s' of in-port '%s' of block '%s'.",
					    port_ptr->in_type_name, port_ptr->name, b->name);
					goto out;
				}
				port_ptr->in_type=typ;
			}

			/* out-type */
			if(port_ptr->out_type_name) {
				if((typ = ubx_type_get(ni, port_ptr->out_type_name)) == NULL) {
					ERR("failed to resolve type '%s' of out-port '%s' of block '%s'.",
					    port_ptr->out_type_name, port_ptr->name, b->name);
					goto out;
				}
				port_ptr->out_type=typ;
			}
		}
	}

	/* configs */
	if(b->configs!=NULL) {
		for(config_ptr=b->configs; config_ptr->name!=NULL; config_ptr++) {
			if((typ=ubx_type_get(ni, config_ptr->type_name)) == NULL)  {
				ERR("failed to resolve type '%s' of config '%s' of block '%s'.",
				    config_ptr->type_name, config_ptr->name, b->name);
				goto out;
			}
			config_ptr->value.type=typ;
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
ubx_data_t* __ubx_data_alloc(ubx_type_t* typ, unsigned long array_len)
{
	ubx_data_t* d = NULL;

	if(typ==NULL) {
		ERR("invalid type parameter");
		goto out;
	}

	if(array_len == 0) {
		ERR("invalid array_len 0");
		goto out;
	}

	if((d=calloc(1, sizeof(ubx_data_t)))==NULL)
		goto out_nomem;

	d->type = typ;
	d->len = array_len;

	if((d->data=calloc(array_len, typ->size))==NULL)
		goto out_nomem;

	/* all ok */
	goto out;

 out_nomem:
	ERR("memory allocation failed");
	if(d) free(d);

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
ubx_data_t* ubx_data_alloc(ubx_node_info_t *ni, const char* typname, unsigned long array_len)
{
	ubx_type_t* t = NULL;
	ubx_data_t* d = NULL;

	if(ni==NULL) {
		ERR("ni parameter NULL");
		goto out;
	}

	if((t=ubx_type_get(ni, typname))==NULL) {
		ERR("unknown type '%s'", typname);
		goto out;
	}

	d = __ubx_data_alloc(t, array_len);

	/* all ok */
	goto out;
 out:
	return d;
}

int ubx_data_resize(ubx_data_t *d, unsigned int newlen)
{
	int ret=-1;
	void *ptr;
	unsigned int newsz = newlen * d->type->size;

	if((ptr=realloc(d->data, newsz))==NULL)
		goto out;

	d->data=ptr;
	d->len=newlen;
	ret=0;
 out:
	return ret;
}


/**
 * Free a previously allocated ubx_data_t type.
 *
 * @param ni
 * @param d
 */
void ubx_data_free(ubx_node_info_t *ni, ubx_data_t* d)
{
	free(d->data);
	free(d);
}

/**
 * Copy the size bytes of src into the dest data's buffer.
 *
 * @param dest destination ubx_data_t pointer
 * @param src src data pointer
 * @param size size in bytes to copy intop des
 *
 * @return actual copied data in array units
 */
int data_copy(ubx_data_t *dest, void *src, size_t size)
{
	int ret=0;
	unsigned int dest_len = data_size(dest);

	if(dest_len < size) {
		ERR("provided data buffer too small (is %d, required: %zu)", dest_len, size);
		goto out;
	}

#ifdef CONFIG_PARANOIA
	/* paranoid check */
	if(size % dest->type->size != 0) {
		ERR("size not a multiple of destination type size");
		goto out;
	}
#endif

	memcpy(dest->data, src, size);

	/* compute the actual new array length */
	ret = size / dest->type->size;

 out:
	return ret;
}


/**
 * Assign a ubx_data_t value to an second.
 *
 * @param tgt
 * @param src
 *
 * @return 0 if OK, else -1
 */
int ubx_data_assign(ubx_data_t *tgt, ubx_data_t *src)
{
	int ret=-1;

	if(src->type != tgt->type) {
		ERR("type mismatch: %s <-> %s", get_typename(tgt), get_typename(src));
		goto out;
	}

	if(src->type->type_class != TYPE_CLASS_BASIC &&
	   src->type->type_class != TYPE_CLASS_STRUCT) {
		ERR("can only assign TYPE_CLASS_[BASIC|STRUCT]");
		goto out;
	}

	if(tgt->len != tgt->len) {
		ERR("length mismatch: %lu <-> %lu", tgt->len, src->len);
		goto out;
	}

	memcpy(tgt->data, src->data, data_size(tgt));
	ret=0;
 out:
	return ret;
}

/**
 * Calculate the size in bytes of a ubx_data_t buffer.
 *
 * @param d
 *
 * @return length in bytes
 */
unsigned int data_size(ubx_data_t* d)
{
	if(d==NULL) {
		ERR("data is NULL");
		goto out_err;
	}

	if(d->type==NULL) {
		ERR("data->type is NULL");
		goto out_err;
	}

	if(d->len * d->type->size == 0)
		ERR("0!! %s, len=%ld, size=%ld", d->type->name, d->len, d->type->size);

	return d->len * d->type->size;

 out_err:
	return 0;
}

int ubx_num_blocks(ubx_node_info_t* ni) { return HASH_COUNT(ni->blocks); }
int ubx_num_types(ubx_node_info_t* ni) { return HASH_COUNT(ni->types); }
int ubx_num_modules(ubx_node_info_t* ni) { return HASH_COUNT(ni->modules); }

/**
 * ubx_port_free_data - free additional memory used by port.
 *
 * @param p port pointer
 */
void ubx_port_free_data(ubx_port_t* p)
{
	if(p->out_type_name) free((char*) p->out_type_name);
	if(p->in_type_name) free((char*) p->in_type_name);

	if(p->in_interaction) free((char*) p->in_interaction);
	if(p->out_interaction) free((char*) p->out_interaction);

	if(p->doc) free((char*) p->doc);
	if(p->name) free((char*) p->name);

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
int ubx_clone_port_data(ubx_port_t *p, const char* name, const char* doc,
			ubx_type_t* in_type, unsigned long in_data_len,
			ubx_type_t* out_type, unsigned long out_data_len, uint32_t state)
{
	int ret = EOUTOFMEM;

	if(!name) {
		ERR("port name is mandatory");
		goto out;
	}

	memset(p, 0x0, sizeof(ubx_port_t));

	if((p->name=strdup(name))==NULL)
		goto out_err;

	if(doc)
		if((p->doc=strdup(doc))==NULL)
			goto out_err_free;

	if(in_type!=NULL) {
		if((p->in_type_name=strdup(in_type->name))==NULL)
			goto out_err_free;
		p->in_type=in_type;
		p->attrs|=PORT_DIR_IN;
	}

	if(out_type!=NULL) {
		if((p->out_type_name=strdup(out_type->name))==NULL)
			goto out_err_free;
		p->out_type=out_type;
		p->attrs|=PORT_DIR_OUT;
	}

	p->in_data_len = (in_data_len==0) ? 1 : in_data_len;
	p->out_data_len = (out_data_len==0) ? 1 : out_data_len;

	p->state=state;

	/* all ok */
	ret=0;
	goto out;

 out_err_free:
	ubx_port_free_data(p);
 out_err:
	ERR("out of memory");
 out:
	return ret;
}

/**
 * ubx_clear_config_data - free a config's extra memory
 *
 * @param c config whose data to free
 */
static void ubx_config_free_data(ubx_config_t *c)
{
	if(c->name) free((char*) c->name);
	if(c->doc) free((char*) c->doc);
	if(c->type_name) free((char*) c->type_name);
	if(c->value.data) free(c->value.data);
	memset(c, 0x0, sizeof(ubx_config_t));
}

/**
 * ubx_clone_conf_data - clone the additional config data
 *
 * @param cnew pointer to (allocated) ubx_config
 * @param name name of configuration
 * @param type_name name of contained type
 * @param len array size of data
 *
 * @return less than zero if an error occured, 0 otherwise.
 *
 * This function allocates memory.
 */
static int ubx_clone_config_data(ubx_config_t *cnew,
				 const char* name,
				 const char* doc,
				 const ubx_type_t* type,
				 unsigned long len)
{
	memset(cnew, 0x0, sizeof(ubx_config_t));

	if((cnew->name=strdup(name))==NULL)
		goto out_err;

	if(doc)
		if((cnew->doc=strdup(doc))==NULL)
			goto out_err;

	if((cnew->type_name=strdup(type->name))==NULL)
		goto out_err;

	cnew->value.type = type;
	cnew->value.len=(len==0) ? 1 : len;

	/* alloc actual buffer */
	cnew->value.data = calloc(cnew->value.len, cnew->value.type->size);

	return 0; /* all ok */

 out_err:
	ubx_config_free_data(cnew);
	return -1;
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
	ubx_port_t *port_ptr;
	ubx_config_t *config_ptr;

	/* configs */
	if(b->configs!=NULL) {
		for(config_ptr=b->configs; config_ptr->name!=NULL; config_ptr++)
			ubx_config_free_data(config_ptr);

		free(b->configs);
	}

	/* ports */
	if(b->ports!=NULL) {
		for(port_ptr=b->ports; port_ptr->name!=NULL; port_ptr++)
			ubx_port_free_data(port_ptr);

		free(b->ports);
	}

	if(b->prototype) free(b->prototype);
	if(b->meta_data) free((char*) b->meta_data);
	if(b->name) free((char*) b->name);
	if(b) free(b);
}


/**
 * ubx_block_clone - create a copy of an existing block from an existing one.
 *
 * @param prot prototype block which to clone
 * @param name name of new block
 *
 * @return a pointer to the newly allocated block. Must be freed with
 */
static ubx_block_t* ubx_block_clone(ubx_block_t* prot, const char* name)
{
	int i;
	ubx_block_t *newb;
	ubx_port_t *srcport, *tgtport;
	ubx_config_t *srcconf, *tgtconf;

	if((newb = calloc(1, sizeof(ubx_block_t)))==NULL)
		goto out_free;

	newb->block_state = BLOCK_STATE_PREINIT;

	/* copy attributes of prototype */
	newb->type = prot->type;

	if((newb->name=strdup(name))==NULL) goto out_free;
	if((newb->meta_data=strdup(prot->meta_data))==NULL) goto out_free;
	if((newb->prototype=strdup(prot->name))==NULL) goto out_free;

	/* copy configuration */
	if(prot->configs) {
		for(i=0; prot->configs[i].name!=NULL; i++); /* compute length */

		if((newb->configs = calloc(i+1, sizeof(ubx_config_t)))==NULL)
			goto out_free;

		for(srcconf=prot->configs, tgtconf=newb->configs; srcconf->name!=NULL; srcconf++,tgtconf++) {
			if(ubx_clone_config_data(tgtconf, srcconf->name, srcconf->doc,
						 srcconf->value.type, srcconf->value.len) != 0)
				goto out_free;
		}
	}


	/* do we have ports? */
	if(prot->ports) {
		for(i=0; prot->ports[i].name!=NULL; i++); /* find number of ports */

		if((newb->ports = calloc(i+1, sizeof(ubx_port_t)))==NULL)
			goto out_free;

		for(srcport=prot->ports, tgtport=newb->ports; srcport->name!=NULL; srcport++,tgtport++) {

			if(ubx_clone_port_data(tgtport, srcport->name, srcport->doc,
					       srcport->in_type, srcport->in_data_len,
					       srcport->out_type, srcport->out_data_len,
					       srcport->state) != 0)
				goto out_free;

			tgtport->block = newb;
		}
	}

	/* ops */
	newb->init=prot->init;
	newb->start=prot->start;
	newb->stop=prot->stop;
	newb->cleanup=prot->cleanup;

	/* copy special ops */
	switch(prot->type) {
	case BLOCK_TYPE_COMPUTATION:
		newb->step=prot->step;
		newb->stat_num_steps = 0;
		break;
	case BLOCK_TYPE_INTERACTION:
		newb->read=prot->read;
		newb->write=prot->write;
		newb->stat_num_reads = 0;
		newb->stat_num_writes = 0;
		break;
	}

	/* all ok */
	return newb;

 out_free:
	ubx_block_free(newb);
	ERR("insufficient memory");
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
ubx_block_t* ubx_block_create(ubx_node_info_t *ni, const char *type, const char* name)
{
	ubx_block_t *prot, *newb;
	newb=NULL;

	if(name==NULL) {
		ERR("name is NULL");
		goto out;
	}

	/* find prototype */
	HASH_FIND_STR(ni->blocks, type, prot);

	if(prot==NULL) {
		ERR("no block with name '%s' found", type);
		goto out;
	}

	/* check if name is already used */
	HASH_FIND_STR(ni->blocks, name, newb);

	if(newb!=NULL) {
		ERR("existing block named '%s'", name);
		newb=NULL;
		goto out;
	}

	if((newb=ubx_block_clone(prot, name))==NULL)
		goto out;

	/* register block */
	if(ubx_block_register(ni, newb) !=0){
		ERR("failed to register block %s", name);
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
int ubx_block_rm(ubx_node_info_t *ni, const char* name)
{
	int ret=-1;
	ubx_block_t *b;

	b = ubx_block_get(ni, name);

	if(b==NULL) {
		ERR("no block named '%s'", name);
		ret = ENOSUCHBLOCK;
		goto out;
	}

	if(b->prototype==NULL) {
		ERR("block '%s' is a prototype", name);
		goto out;
	}

	if(b->block_state!=BLOCK_STATE_PREINIT) {
		ERR("block '%s' not in preinit state", name);
		goto out;
	}

	if(ubx_block_unregister(ni, name)==NULL){
		ERR("block '%s' failed to unregister", name);
	}

	ubx_block_free(b);
	ret=0;
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
	unsigned long newlen; /* new length of array including NULL element */
	ubx_block_t **tmpb;

	/* determine newlen
	 * with one element, the array is size two to hold the terminating NULL element.
	 */
	if(*arr==NULL)
		newlen=2;
	else
		for(tmpb=*arr, newlen=2; *tmpb!=NULL; tmpb++,newlen++);

	if((*arr=realloc(*arr, sizeof(ubx_block_t*) * newlen))==NULL) {
		ERR("insufficient memory");
		ret=EOUTOFMEM;
		goto out;
	}

	DBG("newlen %ld, *arr=%p", newlen, *arr);
	(*arr)[newlen-2]=newblock;
	(*arr)[newlen-1]=NULL;
	ret=0;
 out:
	return ret;
}

static int array_block_rm(ubx_block_t ***arr, ubx_block_t *rmblock)
{
	int ret=-1, cnt, match=-1;
	ubx_block_t **tmpb;

	for(tmpb=*arr,cnt=0; *tmpb!=NULL; tmpb++,cnt++) {
		if (*tmpb==rmblock)
			match=cnt;
	}

	if(match==-1) {
		ERR("no block %s found", rmblock->name);
		goto out;
	}

	/* ERR("match=%d, total=%d", match, cnt); */

	/* case 1: remove last */
	if (match==cnt-1)
		(*arr)[match]=NULL;
	else {
		(*arr)[match]=(*arr)[cnt-1];
		(*arr)[cnt-1]=NULL;
	}

	ret=0;
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
int ubx_port_connect_out(ubx_port_t* p, ubx_block_t* iblock)
{
	int ret=-1;

	if(p->attrs & PORT_DIR_OUT) {
		if((ret=array_block_add(&p->out_interaction, iblock))!=0) {
			ERR("failed to connect port %s out-channel to interaction %s", p->name, iblock->name);
			goto out;
		}
	} else {
		ERR("port %s is not an out port", p->name);
		goto out;
	}

	/* all ok */
	ret=0;
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

int ubx_port_connect_in(ubx_port_t* p, ubx_block_t* iblock)
{
	int ret=-1;

	if(p->attrs & PORT_DIR_IN) {
		if((ret=array_block_add(&p->in_interaction, iblock))!=0) {
			ERR("failed to connect port %s in-channel to interaction %s", p->name, iblock->name);
			goto out;
		}
	} else {
		ERR("port %s is not an in port", p->name);
		goto out;
	}

	/* all ok */
	ret=0;
out:
	return ret;
}

/**
 * ubx_ports_connect_uni - connect a port out- to a port in-channel using an interaction iblock.
 *
 * @param out_port
 * @param in_port
 * @param iblock
 *
 * @return < 0  in case of error, 0 otherwise.
 */
int ubx_ports_connect_uni(ubx_port_t* out_port, ubx_port_t* in_port, ubx_block_t* iblock)
{
	int ret=-1;

	if(iblock==NULL) {
		ERR("iblock NULL");
		goto out;
	}

	if(out_port==NULL) {
		ERR("out_port NULL");
		goto out;
	}

	if(in_port==NULL) {
		ERR("in_port NULL");
		goto out;
	}

	if(iblock->type != BLOCK_TYPE_INTERACTION) {
		ERR("block not of type interaction");
		goto out;
	}

	if((ret=ubx_port_connect_out(out_port, iblock))!=0) goto out;
	if((ret=ubx_port_connect_in(in_port, iblock))!=0) goto out;

	ret=0;
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
int ubx_port_disconnect_out(ubx_port_t* out_port, ubx_block_t* iblock)
{
	int ret=-1;
	if(out_port->attrs & PORT_DIR_OUT) {
		if((ret=array_block_rm(&out_port->out_interaction, iblock))!=0)
			goto out;
	} else {
		ERR("port %s is not an out-port", out_port->name);
		goto out;
	}
	ret=0;
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
int ubx_port_disconnect_in(ubx_port_t* in_port, ubx_block_t* iblock)
{
	int ret=-1;
	if(in_port->attrs & PORT_DIR_IN) {
		if((ret=array_block_rm(&in_port->in_interaction, iblock))!=0)
			goto out;
	} else {
		ERR("port %s is not an in-port", in_port->name);
		goto out;
	}
	ret=0;
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
int ubx_ports_disconnect_uni(ubx_port_t* out_port, ubx_port_t* in_port, ubx_block_t* iblock)
{
	int ret=-1;

	if(iblock==NULL) {
		ERR("iblock NULL");
		goto out;
	}

	if(out_port==NULL) {
		ERR("out_port NULL");
		goto out;
	}

	if(in_port==NULL) {
		ERR("in_port NULL");
		goto out;
	}

	if(iblock->type != BLOCK_TYPE_INTERACTION) {
		ERR("block not of type interaction");
		goto out;
	}

	if((ret=ubx_port_disconnect_out(out_port, iblock))!=0) goto out;
	if((ret=ubx_port_disconnect_in(in_port, iblock))!=0) goto out;

	ret=0;
out:
	return ret;
}


/*
 * Configuration
 */
static unsigned int get_num_configs(ubx_block_t* b)
{
	unsigned int n;

	if(b->configs==NULL)
		n=0;
	else
		for(n=0; b->configs[n].name!=NULL; n++);

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
ubx_config_t* ubx_config_get(ubx_block_t* b, const char* name)
{
	ubx_config_t* conf = NULL;

	if(b==NULL) {
		ERR("block is NULL");
		goto out;
	}

	if(b->configs==NULL || name==NULL)
		goto out;

	for(conf=b->configs; conf->name!=NULL; conf++)
		if(strcmp(conf->name, name)==0)
			goto out;
	ERR("block %s has no config %s", b->name, name);
	conf=NULL;
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
ubx_data_t* ubx_config_get_data(ubx_block_t* b, const char *name)
{
	ubx_config_t *conf;
	ubx_data_t *data=NULL;

	if(b==NULL) {
		ERR("block is NULL");
		goto out;
	}

	if((conf=ubx_config_get(b, name))==NULL)
		goto out;
	data=&conf->value;
 out:
	return data;
}


/**
 * Return the pointer to a configurations ubx_data_t->data pointer.
 *
 * @param b ubx_block
 * @param name name of the requested configuration value
 * @param *len outvalue, the length of the region pointed to (in bytes)
 *
 * @return the array length of the ubx_data
 */
void* ubx_config_get_data_ptr(ubx_block_t *b, const char *name, unsigned int *len)
{
	ubx_data_t *d;
	void *ret = NULL;
	*len=0;

	if(b==NULL) {
		ERR("block is NULL");
		goto out;
	}

	if((d = ubx_config_get_data(b, name))==NULL)
		goto out;

	ret = d->data;
	*len = d->len;
 out:
	return ret;
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
int ubx_config_add(ubx_block_t* b, const char* name, const char* meta, const char *type_name, unsigned long len)
{
	ubx_type_t* typ;
	ubx_config_t* carr;
	int i, ret=-1;


	if(b==NULL) {
		ERR("block is NULL");
		goto out;
	}

	if((typ=ubx_type_get(b->ni, type_name))==NULL) {
		ERR("unkown type '%s'", type_name);
		goto out;
	}

	if(b->configs==NULL)
		i=0;
	else
		for(i=0; b->configs[i].name!=NULL; i++);

	carr=realloc(b->configs, (i+2) * sizeof(ubx_config_t));

	if(carr==NULL) {
		ERR("out of mem, config not added.");
		goto out;
	}

	b->configs=carr;

	if((ret=ubx_clone_config_data(&b->configs[i], name, meta, typ, len)) != 0) {
		ERR("cloning config data failed");
		goto out;
	}

	memset(&b->configs[i+1], 0x0, sizeof(ubx_config_t));

	ret=0;
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
int ubx_config_rm(ubx_block_t* b, const char* name)
{
	int ret=-1, i, num_configs;

	if(!b) {
		ERR("block is NULL");
		goto out;
	}

	if(b->prototype==NULL) {
		ERR("modifying prototype block not allowed");
		goto out;
	}

	if(b->configs==NULL) {
		ERR("no config '%s' found", name);
		goto out;
	}

	num_configs=get_num_configs(b);

	for(i=0; i<num_configs; i++)
		if(strcmp(b->configs[i].name, name)==0)
			break;

	if(i>=num_configs) {
		ERR("no config %s found", name);
		goto out;
	}

	ubx_config_free_data(&b->configs[i]);

	if(i<num_configs-1) {
		b->configs[i]=b->configs[num_configs-1];
		memset(&b->configs[num_configs-1], 0x0, sizeof(ubx_config_t));
	}

	ret=0;
 out:
	return ret;
}


/*
 * Ports
 */

static unsigned int get_num_ports(ubx_block_t* b)
{
	unsigned int n;

	if(b->ports==NULL)
		n=0;
	else
		for(n=0; b->ports[n].name!=NULL; n++); /* find number of ports */

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
int ubx_port_add(ubx_block_t* b, const char* name, const char* doc,
	     const char* in_type_name, unsigned long in_data_len,
	     const char* out_type_name, unsigned long out_data_len, uint32_t state)
{
	int i, ret=-1;
	ubx_port_t* parr;
	ubx_type_t *in_type=NULL, *out_type=NULL;

	if(b==NULL) {
		ERR("block is NULL");
		goto out;
	}

	if(b->prototype==NULL) {
		ERR("modifying prototype block not allowed");
		goto out;
	}

	if(in_type_name) {
		if((in_type = ubx_type_get(b->ni, in_type_name))==NULL) {
			ERR("failed to resolve in_type '%s'", in_type_name);
			goto out;
		}
	}

	if(out_type_name) {
		if((out_type = ubx_type_get(b->ni, out_type_name))==NULL) {
			ERR("failed to resolve out_type '%s'", out_type_name);
			goto out;
		}
	}

	i=get_num_ports(b);

	parr=realloc(b->ports, (i+2) * sizeof(ubx_port_t));

	if(parr==NULL) {
		ERR("out of mem, port not added.");
		goto out;
	}

	b->ports=parr;

	/* setup port */
	b->ports[i].block = b;

	ret=ubx_clone_port_data(&b->ports[i], name, doc,
				in_type, in_data_len,
				out_type, out_data_len, state);

	if(ret) {
		ERR("cloning port data failed");
		memset(&b->ports[i], 0x0, sizeof(ubx_port_t));
		/* nothing else to cleanup, really */
	}

	/* set dummy stopper to NULL */
	memset(&b->ports[i+1], 0x0, sizeof(ubx_port_t));
 out:
	return ret;
}

/**
 * ubx_port_rm - remove a port from a block.
 *
 * @param b
 * @param name
 *
 * @return
 */
int ubx_port_rm(ubx_block_t* b, const char* name)
{
	int ret=-1, i, num_ports;

	if(!b) {
		ERR("block is NULL");
		goto out;
	}

	if(b->prototype==NULL) {
		ERR("modifying prototype block not allowed");
		goto out;
	}

	if(b->ports==NULL) {
		ERR("no port '%s' found", name);
		goto out;
	}

	num_ports=get_num_ports(b);

	for(i=0; i<num_ports; i++)
		if(strcmp(b->ports[i].name, name)==0)
			break;

	if(i>=num_ports) {
		ERR("no port %s found", name);
		goto out;
	}

	ubx_port_free_data(&b->ports[i]);

	/* if the removed port is the last in list, there is nothing
	 * to do. if not, copy the last port into the removed one [i]
	 * and zero last one */
	if(i<num_ports-1) {
		b->ports[i]=b->ports[num_ports-1];
		memset(&b->ports[num_ports-1], 0x0, sizeof(ubx_port_t));
	}

	ret=0;
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
ubx_port_t* ubx_port_get(ubx_block_t* b, const char *name)
{
	ubx_port_t *port_ptr=NULL;

	if(b==NULL) {
		ERR("block is NULL");
		goto out;
	}

	if(name==NULL) {
		ERR("name is NULL");
		goto out;
	}

	if(b->ports==NULL)
		goto out_notfound;

	for(port_ptr=b->ports; port_ptr->name!=NULL; port_ptr++)
		if(strcmp(port_ptr->name, name)==0)
			goto out;
 out_notfound:
	ERR("block %s has no port %s", b->name, name);
	port_ptr=NULL;
 out:
	return port_ptr;
}


/**
 * ubx_block_init - initalize a function block.
 *
 * @param ni
 * @param b
 *
 * @return 0 if state was changed, -1 otherwise.
 */
int ubx_block_init(ubx_block_t* b)
{
	int ret = -1;

	if(b==NULL) {
		ERR("block is NULL");
		goto out;
	}

	if(b->block_state!=BLOCK_STATE_PREINIT) {
		ERR("block '%s' not in state preinit, but in %s",
		    b->name, block_state_tostr(b->block_state));
		goto out;
	}

	if(b->init==NULL) goto out_ok;

	if(b->init(b)!=0) {
		ERR("block '%s' init function failed.", b->name);
		goto out;
	}

 out_ok:
	b->block_state=BLOCK_STATE_INACTIVE;
	ret=0;
 out:
	return ret;
}

/**
 * ubx_block_start - start a function block.
 *
 * @param ni
 * @param b
 *
 * @return 0 if state was changed, -1 otherwise.
 */
int ubx_block_start(ubx_block_t* b)
{
	int ret = -1;

	if(b==NULL) {
		ERR("block is NULL");
		goto out;
	}

	if(b->block_state!=BLOCK_STATE_INACTIVE) {
		ERR("block '%s' not in state inactive, but in %s",
		    b->name, block_state_tostr(b->block_state));
		goto out;
	}

	if(b->start==NULL) goto out_ok;

	if(b->start(b)!=0) {
		ERR("block '%s' start function failed.", b->name);
		goto out;
	}

 out_ok:
	b->block_state=BLOCK_STATE_ACTIVE;
	ret=0;
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
int ubx_block_stop(ubx_block_t* b)
{
	int ret = -1;

	if(b==NULL) {
		ERR("block is NULL");
		goto out;
	}

	if(b->block_state!=BLOCK_STATE_ACTIVE) {
		ERR("block '%s' not in state active, but in %s",
		    b->name, block_state_tostr(b->block_state));
		goto out;
	}

	if(b->stop==NULL) goto out_ok;

	b->stop(b);

 out_ok:
	b->block_state=BLOCK_STATE_INACTIVE;
	ret=0;
 out:
	return ret;
}

/**
 * ubx_block_cleanup - bring function block back to preinit state.
 *
 * @param ni
 * @param b
 *
 * @return 0 if state was changed, -1 otherwise.
 */
int ubx_block_cleanup(ubx_block_t* b)
{
	int ret=-1;

	if(b==NULL) {
		ERR("block is NULL");
		goto out;
	}

	if(b->block_state!=BLOCK_STATE_INACTIVE) {
		ERR("block '%s' not in state inactive, but in %s",
		    b->name, block_state_tostr(b->block_state));
		goto out;
	}

	if(b->cleanup==NULL) goto out_ok;

	b->cleanup(b);

 out_ok:
	b->block_state=BLOCK_STATE_PREINIT;
	ret=0;
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
int ubx_cblock_step(ubx_block_t* b)
{
	int ret = -1;

	if(b==NULL) {
		ERR("block is NULL");
		goto out;
	}

	if(b->type!=BLOCK_TYPE_COMPUTATION) {
		ERR("block %s: can't step block of type %u", b->name, b->type);
		goto out;
	}

	if(b->block_state!=BLOCK_STATE_ACTIVE) {
		ERR("block %s not active", b->name);
		goto out;
	}

	if(b->step==NULL) goto out;

	b->step(b);
	b->stat_num_steps++;
	ret=0;
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
uint32_t __port_read(ubx_port_t* port, ubx_data_t* data)
{
	uint32_t ret=0;
	const char *tp;
	ubx_block_t **iaptr;

	if (!port) {
		ERR("port null");
		ret = EPORT_INVALID;
		goto out;
	};

	if(!data) {
		ERR("data null");
		ret = -1;
		goto out;
	};

	if(data->len<=0) {
		ERR("data->len is <=0");
		ret = -1;
		goto out;
	}

	if((port->attrs & PORT_DIR_IN) == 0) {
		ERR("not an IN-port");
		ret = EPORT_INVALID_TYPE;
		goto out;
	};

	if(port->in_type != data->type) {
		tp=get_typename(data);
		ERR("port %s, mismatching types, data: %s, port: %s",
		    port->name, tp, port->in_type->name);
		goto out;
	}

	/* port completely unconnected? */
	if(port->in_interaction==NULL)
		goto out;

	for(iaptr=port->in_interaction; *iaptr!=NULL; iaptr++) {
		if((*iaptr)->block_state==BLOCK_STATE_ACTIVE) {
			if((ret=(*iaptr)->read(*iaptr, data)) > 0) {
				port->stat_reades++;
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
void __port_write(ubx_port_t* port, ubx_data_t* data)
{
	/* int i; */
	const char *tp;
	ubx_block_t **iaptr;

	if (port==NULL) {
		ERR("port null");
		goto out;
	};

	if ((port->attrs & PORT_DIR_OUT) == 0) {
		ERR("not an OUT-port");
		goto out;
	};

	if(port->out_type != data->type) {
		tp=get_typename(data);
		ERR("port %s, mismatching types, data: %s, port: %s",
		    port->name, tp, port->out_type->name);
		goto out;
	}

	/* port completely unconnected? */
	if(port->out_interaction==NULL)
		goto out;

	/* pump it out */
	for(iaptr=port->out_interaction; *iaptr!=NULL; iaptr++) {
		if((*iaptr)->block_state==BLOCK_STATE_ACTIVE) {
			DBG("writing to interaction '%s'", (*iaptr)->name);
			(*iaptr)->write(*iaptr, data);
			(*iaptr)->stat_num_writes++;
		}
	}

	/* above looks nicer */
	/* for(i=0; port->out_interaction[i]!=NULL; i++) */
	/*	port->out_interaction[i]->write(port->out_interaction[i], data); */

	port->stat_writes++;
 out:
	return;
}


/* OS stuff, for scripting layer */

/**
 * ubx_clock_mono_gettime - get current time using clock_gettime(CLOCK_MONOTONIC).
 *
 * @param uts
 *
 * @return < 0 in case of error, 0 otherwise.
 */
int ubx_clock_mono_gettime(struct ubx_timespec* uts)
{
	int ret=-1;
	struct timespec ts;

	if(uts==NULL) {
		ERR("struct ubx_timespec argument missing");
		goto out;
	}

	if(clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
		ERR2(errno, "clock_gettime failed");
		goto out;
	}
	uts->sec=ts.tv_sec;
	uts->nsec=ts.tv_nsec;
	ret=0;
out:
	return ret;
}

/**
 * Compare two ubx_timespecs
 *
 * @param ts1
 * @param ts2
 *
 * @return return 1 if t1 is greater than t2, -1 if t1 is less than t2 and 0 if t1 and t2 are equal.
 */
int ubx_ts_cmp(struct ubx_timespec *ts1, struct ubx_timespec *ts2)
{
	if (ts1->sec > ts2->sec) return 1;
	else if (ts1->sec < ts2->sec) return -1;
	else if (ts1->nsec > ts2->nsec) return 1;
	else if (ts1->nsec < ts2->nsec) return -1;
	else return 0;
}

/**
 * Normalize a timespec.
 *
 * @param ts
 */
void ubx_ts_norm(struct ubx_timespec *ts)
{

	if(ts->nsec >= NSEC_PER_SEC) {
		ts->sec+=ts->nsec / NSEC_PER_SEC;
		ts->nsec=ts->nsec % NSEC_PER_SEC;
	}

	/* normalize negative nsec */
	if(ts->nsec <= -NSEC_PER_SEC) {
		ts->sec += ts->nsec / NSEC_PER_SEC;
		ts->nsec=ts->nsec % -NSEC_PER_SEC;
	}

	if(ts->sec>0 && ts->nsec<0) {
		ts->sec-=1;
		ts->nsec+=NSEC_PER_SEC;
	}

	if(ts->sec<0 && ts->nsec>0) {
		ts->sec++;
		ts->nsec-=NSEC_PER_SEC;
	}
}


/**
 * ubx_ts_sub - substract ts2 from ts1 and store the result in out
 *
 * @param ts1
 * @param ts2
 * @param out
 */
void ubx_ts_sub(struct ubx_timespec *ts1, struct ubx_timespec *ts2, struct ubx_timespec *out)
{

	out->sec = ts1->sec - ts2->sec;
	out->nsec = ts1->nsec - ts2->nsec;
	ubx_ts_norm(out);
}

/**
 * ubx_ts_add - compute the sum of two timespecs and store the result in out
 *
 * @param ts1
 * @param ts2
 * @param out
 */
void ubx_ts_add(struct ubx_timespec *ts1, struct ubx_timespec *ts2, struct ubx_timespec *out)
{
	out->sec = ts1->sec + ts2->sec;
	out->nsec = ts1->nsec + ts2->nsec;
	ubx_ts_norm(out);
}

/**
 * ubx_ts_div - divide the value of ts by div and store the result in out
 *
 * @param ts
 * @param div
 * @param out
 */
void ubx_ts_div(struct ubx_timespec *ts, long div, struct ubx_timespec *out)
{
	out->sec = ts->sec/div;
	out->nsec = ts->sec/div;
}

/**
 * ubx_ts_to_double - convert ubx_timespec to double [s]
 *
 * @param ts
 *
 * @return time in seconds
 */
double ubx_ts_to_double(struct ubx_timespec *ts)
{
	return ((double) ts->sec) + ((double) ts->nsec/NSEC_PER_SEC);
}
