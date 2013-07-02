
#define DEBUG 1

#include "u5c.h"

/*
 * Internal helper functions
 */

/* for pretty printing */
const char *block_states[] = {	"preinit", "inactive", "active" };

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
const char* get_typename(u5c_data_t *data)
{
	if(data && data->type)
		return data->type->name;
	return NULL;
}

/**
 * initalize node_info
 *
 * @param ni
 *
 * @return
 */
int u5c_node_init(u5c_node_info_t* ni, const char *name)
{
	/* if(mlockall(MCL_CURRENT | MCL_FUTURE) != 0) { */
	/* 	ERR2(errno, " "); */
	/* 	goto out_err; */
	/* }; */

	if(name!=NULL) ni->name=strdup(name);

	ni->blocks=NULL;
	ni->types=NULL;
	ni->cur_seqid=0;

	return 0;

	/* out_err: */
	/* return -1; */
}

void u5c_node_cleanup(u5c_node_info_t* ni)
{
	/* clean up all entities */
}


/**
 * u5c_block_register - register a block with the given node_info.
 *
 * @param ni
 * @param block
 *
 * @return 0 if Ok, < 0 otherwise.
 */
int u5c_block_register(u5c_node_info_t *ni, u5c_block_t* block)
{
	int ret = 0;
	u5c_block_t *tmpc;

	if(block->type!=BLOCK_TYPE_COMPUTATION &&
	   block->type!=BLOCK_TYPE_INTERACTION &&
	   block->type!=BLOCK_TYPE_TRIGGER) {
		ERR("invalid block type %d", block->type);
		ret=-1;
		goto out;
	}

	/* TODO: consistency check */

	HASH_FIND_STR(ni->blocks, block->name, tmpc);

	if(tmpc!=NULL) {
		ERR("block with name '%s' already registered.", block->name);
		ret=-1;
		goto out;
	};

	/* resolve types */
	if((ret=u5c_resolve_types(ni, block)) !=0)
		goto out;

	HASH_ADD_KEYPTR(hh, ni->blocks, block->name, strlen(block->name), block);

 out:
	return ret;
}

/**
 * Retrieve a block by name
 *
 * @param ni
 * @param name
 *
 * @return u5c_block_t*
 */
u5c_block_t* u5c_block_get(u5c_node_info_t *ni, const char *name)
{
	u5c_block_t *tmpc=NULL;
	HASH_FIND_STR(ni->blocks, name, tmpc);
	return tmpc;
}

/**
 * u5c_block_unregister - unregister a block.
 *
 * @param ni
 * @param type
 * @param name
 *
 * @return the unregistered block or NULL in case of failure.
 */
u5c_block_t* u5c_block_unregister(u5c_node_info_t* ni, const char* name)
{
	u5c_block_t *tmpc=NULL;

	HASH_FIND_STR(ni->blocks, name, tmpc);

	if(tmpc==NULL) {
		ERR("component '%s' not registered.", name);
		goto out;
	}

	HASH_DEL(ni->blocks, tmpc);
 out:
	return tmpc;
}


/**
 * u5c_type_register - register a type with a node.
 *
 * @param ni
 * @param type
 *
 * @return
 */
int u5c_type_register(u5c_node_info_t* ni, u5c_type_t* type)
{
	int ret = -1;
	u5c_type_t* typ;

	if(type==NULL) {
		ERR("given type is NULL");
		goto out;
	}

	/* TODO consistency checkm type->name must exists,... */
	HASH_FIND_STR(ni->types, type->name, typ);

	if(typ!=NULL) {
		/* check if types are the same, if yes no error */
		ERR("type '%s' already registered.", type->name);
		goto out;
	};

	HASH_ADD_KEYPTR(hh, ni->types, type->name, strlen(type->name), type);
	type->seqid=ni->cur_seqid++;
	ret = 0;
 out:
	return ret;
}

/**
 * u5c_type_unregister - unregister type with node
 *
 * TODO: use count handling, only succeed unloading when not used!
 *
 * @param ni
 * @param name
 *
 * @return
 */
u5c_type_t* u5c_type_unregister(u5c_node_info_t* ni, const char* name)
{
	u5c_type_t* ret = NULL;

	HASH_FIND_STR(ni->types, name, ret);

	if(ret==NULL) {
		ERR("no type '%s' registered.", name);
		goto out;
	};

	HASH_DEL(ni->types, ret);
 out:
	return ret;
}

/**
 * u5c_type_get - find a u5c_type by name.
 *
 * @param ni
 * @param name
 *
 * @return pointer to u5c_type or NULL if not found.
 */
u5c_type_t* u5c_type_get(u5c_node_info_t* ni, const char* name)
{
	u5c_type_t* ret=NULL;
	HASH_FIND_STR(ni->types, name, ret);
	return ret;
}

/**
 * u5c_resolve_types - resolve string type references to real type object.
 *
 * For each port and config, let the type pointers point to the
 * correct type identified by the char* name. Error if failure.
 *
 * @param ni
 * @param b
 *
 * @return 0 if all types are resolved, -1 if not.
 */
int u5c_resolve_types(u5c_node_info_t* ni, u5c_block_t* b)
{
	int ret = 0;
	u5c_type_t* typ;
	u5c_port_t* port_ptr;
	u5c_config_t *config_ptr;

	/* for each port locate type and resolve */
	if(b->ports) {
		for(port_ptr=b->ports; port_ptr->name!=NULL; port_ptr++) {
			/* in-type */
			if(port_ptr->in_type_name) {
				HASH_FIND_STR(ni->types, port_ptr->in_type_name, typ);
				if(typ==NULL) {
					ERR("failed to resolve type '%s' of in-port '%s' of block '%s'.",
					    port_ptr->in_type_name, port_ptr->name, b->name);
					ret=-1;
					goto out;
				}
				port_ptr->in_type=typ;

			}


			/* out-type */
			if(port_ptr->out_type_name) {
				HASH_FIND_STR(ni->types, port_ptr->out_type_name, typ);
				if(typ==NULL) {
					ERR("failed to resolve type '%s' of out-port '%s' of block '%s'.",
					    port_ptr->out_type_name, port_ptr->name, b->name);
					ret=-1;
					goto out;
				}
				port_ptr->out_type=typ;
			}
		}
	}

	/* configs */
	if(b->configs!=NULL) {
		for(config_ptr=b->configs; config_ptr->name!=NULL; config_ptr++) {
			HASH_FIND_STR(ni->types, config_ptr->type_name, typ);
			if(typ==NULL)  {
				ERR("failed to resolve type '%s' of config '%s' of block '%s'.",
				    config_ptr->type_name, config_ptr->name, b->name);
				ret=-1;
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
 * Allocate a u5c_data_t of the given type and array length.
 *
 * This type should be free'd using the u5c_data_free function.
 *
 * @param ni
 * @param typename
 * @param array_len
 *
 * @return u5c_data_t* or NULL in case of error.
 */
u5c_data_t* u5c_data_alloc(u5c_node_info_t *ni, const char* typename, unsigned long array_len)
{
	u5c_type_t* t = NULL;
	u5c_data_t* d = NULL;

	if(array_len == 0) {
		ERR("invalid array_len 0");
		goto out;
	}

	if((t=u5c_type_get(ni, typename))==NULL) {
		ERR("unknown type '%s'", typename);
		goto out;
	}

	if((d=calloc(1, sizeof(u5c_data_t)))==NULL)
		goto out_nomem;

	d->type = t;
	d->len = array_len;

	if((d->data=calloc(array_len, t->size))==NULL)
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
 * Free a previously allocated u5c_data_t type.
 *
 * @param ni
 * @param d
 */
void u5c_data_free(u5c_node_info_t *ni, u5c_data_t* d)
{
	free(d->data);
	free(d);
}

/**
 * Assign a u5c_data_t value to an second.
 *
 * @param tgt
 * @param src
 *
 * @return 0 if OK, else -1
 */
int u5c_data_assign(u5c_data_t *tgt, u5c_data_t *src)
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

	memcpy(tgt->data, src->data, data_len(tgt));
	ret=0;
 out:
	return ret;
}

/**
 * Calculate the length in bytes of a u5c_data_t buffer.
 *
 * @param d
 *
 * @return length in bytes
 */
unsigned int data_len(u5c_data_t* d)
{
	if(d==NULL) {
		ERR("data is NULL");
		goto out_err;
	}

	if(d->type==NULL) {
		ERR("data->type is NULL");
		goto out_err;
	}

	return d->len * d->type->size;

 out_err:
	return 0;
}

int u5c_num_blocks(u5c_node_info_t* ni) { return HASH_COUNT(ni->blocks); }
int u5c_num_types(u5c_node_info_t* ni) { return HASH_COUNT(ni->types); }

/**
 * u5c_port_free_data - free additional memory used by port.
 *
 * @param p port pointer
 */
static void u5c_port_free_data(u5c_port_t* p)
{
	if(p->out_type_name) free(p->out_type_name);
	if(p->in_type_name) free(p->in_type_name);

	if(p->in_interaction) free(p->in_interaction);
	if(p->out_interaction) free(p->out_interaction);

	if(p->meta_data) free(p->meta_data);
	if(p->name) free(p->name);
}

/**
 * u5c_clone_port_data - clone the additional port data
 *
 * @param psrc pointer to the source port
 * @param pcopy pointer to the existing target port
 *
 * @return less than zero if an error occured, 0 otherwise.
 *
 * This function allocates memory.
 */
static int u5c_clone_port_data(const u5c_port_t *psrc, u5c_port_t *pcopy)
{
	/* DBG("cloning port 0x%lx, '%s'\n", (uint64_t) psrc, psrc->name); */

	memset(pcopy, 0x0, sizeof(u5c_port_t));

	if((pcopy->name=strdup(psrc->name))==NULL)
		goto out_err;

	if(psrc->meta_data)
		if((pcopy->meta_data=strdup(psrc->meta_data))==NULL)
			goto out_err_free;

	if(psrc->in_type_name)
		if((pcopy->in_type_name=strdup(psrc->in_type_name))==NULL)
			goto out_err_free;

	if(psrc->out_type_name)
		if((pcopy->out_type_name=strdup(psrc->out_type_name))==NULL)
			goto out_err_free;

	pcopy->in_data_len = (psrc->in_data_len==0) ? 1 : psrc->in_data_len;
	pcopy->out_data_len = (psrc->out_data_len==0) ? 1 : psrc->out_data_len;

	pcopy->attrs=psrc->attrs;
	pcopy->state=psrc->state;

	/* all ok */
	return 0;

 out_err_free:
	u5c_port_free_data(pcopy);
 out_err:
 	return -1;
}

/**
 * u5c_clear_config_data - free a config's extra memory
 *
 * @param c config whose data to free
 */
static void u5c_free_config_data(u5c_config_t *c)
{
	if(c->name) free(c->name);
	if(c->type_name) free(c->type_name);
	if(c->value.data) free(c->value.data);
}

/**
 * u5c_clone_conf_data - clone the additional config data
 *
 * @param psrc pointer to the source config
 * @param pcopy pointer to the existing target config
 *
 * @return less than zero if an error occured, 0 otherwise.
 *
 * This function allocates memory.
 */
static int u5c_clone_config_data(const u5c_config_t *csrc, u5c_config_t *ccopy)
{
	if(csrc->value.type==NULL) {
		ERR("unresolved source config");
		goto out_err;
	}

	memset(ccopy, 0x0, sizeof(u5c_config_t));

	if((ccopy->name=strdup(csrc->name))==NULL)
		goto out_err;

	if((ccopy->type_name=strdup(csrc->type_name))==NULL)
		goto out_err;

	ccopy->value.type = csrc->value.type;

	/* TODO: fix this during registration */
	ccopy->value.len=(csrc->value.len==0) ? 1 : csrc->value.len;

	/* alloc actual buffer */
	ccopy->value.data = calloc(ccopy->value.len, ccopy->value.type->size);

	return 0; /* all ok */

 out_err:
	u5c_free_config_data(ccopy);
 	return -1;
}


/**
 * u5c_block_free - free all memory related to a block
 *
 * The block should have been previously unregistered.
 *
 * @param ni
 * @param block_type
 * @param name
 *
 * @return
 */
void u5c_block_free(u5c_node_info_t *ni, u5c_block_t *b)
{
	u5c_port_t *port_ptr;
	u5c_config_t *config_ptr;

	/* configs */
	if(b->configs!=NULL) {
		for(config_ptr=b->configs; config_ptr->name!=NULL; config_ptr++)
			u5c_free_config_data(config_ptr);

		free(b->configs);
	}

	/* ports */
	if(b->ports!=NULL) {
		for(port_ptr=b->ports; port_ptr->name!=NULL; port_ptr++)
			u5c_port_free_data(port_ptr);

		free(b->ports);
	}

	if(b->prototype) free(b->prototype);
	if(b->meta_data) free(b->meta_data);
	if(b->name) free(b->name);
	if(b) free(b);
}


/**
 * u5c_block_clone - create a copy of an existing block from an existing one.
 *
 * @param ni node info ptr
 * @param prot prototype block which to clone
 * @param name name of new block
 *
 * @return a pointer to the newly allocated block. Must be freed with
 */
static u5c_block_t* u5c_block_clone(u5c_node_info_t* ni, u5c_block_t* prot, const char* name)
{
	int i;
	u5c_block_t *newb;
	u5c_port_t *srcport, *tgtport;
	u5c_config_t *srcconf, *tgtconf;

	if((newb = calloc(1, sizeof(u5c_block_t)))==NULL)
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

		if((newb->configs = calloc(i+1, sizeof(u5c_config_t)))==NULL)
			goto out_free;

		for(srcconf=prot->configs, tgtconf=newb->configs; srcconf->name!=NULL; srcconf++,tgtconf++) {
			if(u5c_clone_config_data(srcconf, tgtconf) != 0)
				goto out_free;
		}
	}


	/* do we have ports? */
	if(prot->ports) {
		for(i=0; prot->ports[i].name!=NULL; i++); /* find number of ports */

		if((newb->ports = calloc(i+1, sizeof(u5c_port_t)))==NULL)
			goto out_free;

		for(srcport=prot->ports, tgtport=newb->ports; srcport->name!=NULL; srcport++,tgtport++) {
			if(u5c_clone_port_data(srcport, tgtport) != 0)
				goto out_free;
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
	case BLOCK_TYPE_TRIGGER:
		newb->add=prot->add;
		newb->rm=prot->rm;
		break;
	}

	/* all ok */
	return newb;

 out_free:
	u5c_block_free(ni, newb);
 	ERR("insufficient memory");
	return NULL;
}


/**
 *
 *
 * @param ni
 * @param block_type
 * @param name
 *
 * @return
 */
u5c_block_t* u5c_block_create(u5c_node_info_t *ni, const char *type, const char* name)
{
	u5c_block_t *prot, *newb;
	newb=NULL;

	/* find prototype */
	HASH_FIND_STR(ni->blocks, type, prot);

	if(prot==NULL) {
		ERR("no block with name '%s' found", type);
		goto out;
	}

	/* check if name is already used */
	HASH_FIND_STR(ni->blocks, name, newb);

	if(newb!=NULL) {
		ERR("existing component named '%s'", name);
		goto out;
	}

	if((newb=u5c_block_clone(ni, prot, name))==NULL)
		goto out;

	/* register block */
	if(u5c_block_register(ni, newb) !=0){
		ERR("failed to register block %s", name);
		u5c_block_free(ni, newb);
		goto out;
	}
 out:
	return newb;
}

/**
 * u5c_block_rm - unregister and free a block.
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
int u5c_block_rm(u5c_node_info_t *ni, const char* name)
{
	int ret;
	u5c_block_t *b;

	b = u5c_block_get(ni, name);

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

	if(u5c_block_unregister(ni, name)==NULL){
		ERR("block '%s' failed to unregister", name);
	}

	u5c_block_free(ni, b);
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
 * @return
 */
static int array_block_add(u5c_block_t ***arr, u5c_block_t *newblock)
{
	int ret;
	unsigned long newlen; /* new length of array including NULL element */
	u5c_block_t **tmpb;

	/* determine newlen
	 * with one element, the array is size two to hold the terminating NULL element.
	 */
	if(*arr==NULL)
		newlen=2;
	else
		for(tmpb=*arr, newlen=2; *tmpb!=NULL; tmpb++,newlen++);

	if((*arr=realloc(*arr, sizeof(u5c_block_t*) * newlen))==NULL) {
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

/**
 * u5c_connect - connect a port with an interaction.
 *
 * @param p1
 * @param iblock
 *
 * @return
 *
 * This should be made real-time safe by adding an operation
 * u5c_interaction_set_max_conn[in|out]
 */
int u5c_connect_one(u5c_port_t* p, u5c_block_t* iblock)
{
	int ret;

	if(p->attrs & PORT_DIR_IN)
		if((ret=array_block_add(&p->in_interaction, iblock))!=0)
			goto out_err;
	if(p->attrs & PORT_DIR_OUT)
		if((ret=array_block_add(&p->out_interaction, iblock))!=0)
			goto out_err;

	/* all ok */
	goto out;

 out_err:
	ERR("failed to connect port");
 out:
	return ret;
}

/**
 * u5c_connect - connect to ports with a given interaction.
 *
 * @param p1
 * @param p2
 * @param iblock
 *
 * @return
 *
 * This should be made real-time safe by adding an operation
 * u5c_interaction_set_max_conn[in|out]
 */
int u5c_connect(u5c_port_t* p1, u5c_port_t* p2, u5c_block_t* iblock)
{
	int ret;
	if(iblock->type != BLOCK_TYPE_INTERACTION) {
		ERR("block not of type interaction");
		ret=-1;
		goto out;
	}


	if((ret=u5c_connect_one(p1, iblock))!=0) goto out_err;
	if((ret=u5c_connect_one(p2, iblock))!=0) goto out_err;

	/* all ok */
	goto out;

 out_err:
	ERR("failed to connect ports");
 out:
	return ret;
}

/*
 * Configuration
 */

/**
 * u5c_config_get - retrieve a configuration type by name.
 *
 * @param b
 * @param name
 *
 * @return u5c_config_t pointer or NULL if not found.
 */
u5c_config_t* u5c_config_get(u5c_block_t* b, const char* name)
{
	u5c_config_t* conf = NULL;

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
 * u5c_config_get_data - return the data associated with a configuration value.
 *
 * @param b
 * @param name
 *
 * @return u5c_data_t pointer or NULL
 */
u5c_data_t* u5c_config_get_data(u5c_block_t* b, const char *name)
{
	u5c_config_t *conf;
	u5c_data_t *data=NULL;

	if((conf=u5c_config_get(b, name))==NULL)
		goto out;
	data=&conf->value;
 out:
	return data;
}


/**
 * Return the pointer to a configurations u5c_data_t->data pointer.
 *
 * @param b u5c_block
 * @param name name of the requested configuration value
 * @param *len outvalue, the length of the region pointed to (in bytes)
 *
 * @return the lenght in bytes of the pointer
 */
void* u5c_config_get_data_ptr(u5c_block_t *b, const char *name, unsigned int *len)
{
	u5c_data_t *d;
	void *ret = NULL;
	*len=0;

	if((d = u5c_config_get_data(b, name))==NULL)
		goto out;

	ret = d->data;
	*len = data_len(d);
 out:
	return ret;
}


/*
 * Ports
 */

/**
 * u5c_port_get - retrieve a component port by name
 *
 * @param comp
 * @param name
 *
 * @return port pointer or NULL
 */
u5c_port_t* u5c_port_get(u5c_block_t* comp, const char *name)
{
	u5c_port_t *port_ptr;

	if(comp->ports==NULL)
		goto out_notfound;

	for(port_ptr=comp->ports; port_ptr->name!=NULL; port_ptr++)
		if(strcmp(port_ptr->name, name)==0)
			goto out;
 out_notfound:
	port_ptr=NULL;
 out:
	return port_ptr;
}

/* u5c_port_t* u5c_port_add(u5c_block_t* comp, const char *name, const char *type); */
/* u5c_port_t* u5c_port_rm(u5c_block_t* comp); */


/**
 * u5c_block_init - initalize a function block.
 *
 * @param ni
 * @param b
 *
 * @return 0 if state was changed, -1 otherwise.
 */
int u5c_block_init(u5c_node_info_t* ni, u5c_block_t* b)
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
 * u5c_block_start - start a function block.
 *
 * @param ni
 * @param b
 *
 * @return 0 if state was changed, -1 otherwise.
 */
int u5c_block_start(u5c_node_info_t* ni, u5c_block_t* b)
{
	int ret = -1;

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
 * u5c_block_stop - stop a function block
 *
 * @param ni
 * @param b
 *
 * @return
 */
int u5c_block_stop(u5c_node_info_t* ni, u5c_block_t* b)
{
	int ret = -1;

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
 * u5c_block_cleanup - bring function block back to preinit state.
 *
 * @param ni
 * @param b
 *
 * @return 0 if state was changed, -1 otherwise.
 */
int u5c_block_cleanup(u5c_node_info_t* ni, u5c_block_t* b)
{
	int ret=-1;

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
int u5c_cblock_step(u5c_block_t* b)
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
 * @param data u5c_data_t to store result
 *
 * @return status value
 */
uint32_t __port_read(u5c_port_t* port, u5c_data_t* data)
{
	uint32_t ret=PORT_READ_NODATA;
	const char *tp;
	u5c_block_t **iaptr;

	if (!port) {
		ERR("port null");
		ret = EPORT_INVALID;
		goto out;
	};

	if ((port->attrs & PORT_DIR_IN) == 0) {
		ERR("not an IN-port");
		ret = EPORT_INVALID_TYPE;
		goto out;
	};

	if(port->in_type != data->type) {
		tp=get_typename(data);
		ERR("mismatching types, data: %s, port: %s", tp, port->in_type->name);
		goto out;
	}

	/* port completely unconnected? */
	if(port->in_interaction==NULL)
		goto out;

	for(iaptr=port->in_interaction; *iaptr!=NULL; iaptr++) {
		if((*iaptr)->block_state==BLOCK_STATE_ACTIVE) {
			if((ret=(*iaptr)->read(*iaptr, data)) == PORT_READ_NEWDATA) {
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
void __port_write(u5c_port_t* port, u5c_data_t* data)
{
	/* int i; */
	const char *tp;
	u5c_block_t **iaptr;

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
		ERR("mismatching types data: %s, port: %s", tp, port->out_type->name);
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
	/* 	port->out_interaction[i]->write(port->out_interaction[i], data); */

	port->stat_writes++;
 out:
	return;
}
