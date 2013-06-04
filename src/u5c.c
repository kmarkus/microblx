
#define DEBUG 1

#include "u5c.h"

/*
 * Internal helper functions
 */
static u5c_block_t** get_block_list(u5c_node_info_t* ni, uint32_t type)
{
	switch(type) {
	case BLOCK_TYPE_COMPUTATION: return &ni->cblocks;
	case BLOCK_TYPE_INTERACTION: return &ni->iblocks;
	case BLOCK_TYPE_TRIGGER: return &ni->tblocks;
	default: ERR("invalid block type");
	}
	return NULL;
}

const char* get_typename(u5c_data_t *data)
{
	if(data && data->type)
		return data->type->name;
	return NULL;
}

int u5c_node_init(u5c_node_info_t* ni)
{
	/* if(mlockall(MCL_CURRENT | MCL_FUTURE) != 0) { */
	/* 	ERR2(errno, " "); */
	/* 	goto out_err; */
	/* }; */

	ni->cblocks=NULL;
	ni->iblocks=NULL;
	ni->tblocks=NULL;
	ni->types=NULL;

	return 0;

	/* out_err: */
	/* return -1; */
}

void u5c_node_cleanup(u5c_node_info_t* ni)
{
	/* clean up all entities */
}

int u5c_block_register(u5c_node_info_t *ni, u5c_block_t* block)
{
	int ret = 0;
	u5c_block_t *tmpc, **blocklist;

	blocklist = get_block_list(ni, block->type);

	if(blocklist==NULL) {
		ERR("invalid block type %d", block->type);
		ret=-1;
		goto out;
	}

	/* TODO consistency check */

	HASH_FIND_STR(*blocklist, block->name, tmpc);

	if(tmpc!=NULL) {
		ERR("component '%s' already registered.", block->name);
		ret=-1;
		goto out;
	};

	HASH_ADD_KEYPTR(hh, *blocklist, block->name, strlen(block->name), block);

 out:
	return ret;
}

u5c_block_t* u5c_block_unregister(u5c_node_info_t* ni, uint32_t type, const char* name)
{
	u5c_block_t **blocklist, *tmpc=NULL;

	blocklist = get_block_list(ni, type);

	if(blocklist==NULL) {
		ERR("invalid block type %d", type);
		goto out;
	}

	HASH_FIND_STR(*blocklist, name, tmpc);

	if(tmpc==NULL) {
		ERR("no component '%s' registered.", name);
		goto out;
	};

	HASH_DEL(*blocklist, tmpc);
 out:
	return tmpc;
}

int u5c_num_cblocks(u5c_node_info_t* ni) { return HASH_COUNT(ni->cblocks); }
int u5c_num_iblocks(u5c_node_info_t* ni) { return HASH_COUNT(ni->iblocks); }
int u5c_num_tblocks(u5c_node_info_t* ni) { return HASH_COUNT(ni->tblocks); }
int u5c_num_types(u5c_node_info_t* ni) { return HASH_COUNT(ni->types); }

static void u5c_port_free_data(u5c_port_t* p)
{
	if(p->out_type_name) free(p->out_type_name);
	if(p->in_type_name) free(p->in_type_name);
	if(p->meta_data) free(p->meta_data);
	if(p->name) free(p->name);
}

/* create a copy of the given port
 * Function ALLOCATES
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

	pcopy->attrs=psrc->attrs;
	pcopy->state=psrc->state;

	/* all ok */
	return 0;

 out_err_free:
	u5c_port_free_data(pcopy);
 out_err:
 	return -1;
}


/* clone component
 * Function ALLOCATES
 */
u5c_block_t* u5c_block_create(u5c_node_info_t* ni, uint32_t block_type, const char* type, const char* name)
{
	int i;
	u5c_block_t *newc, *prot, **blocklist;
	const u5c_port_t *port_ptr;

	if((blocklist = get_block_list(ni, block_type))==NULL) {
		ERR("invalid block type %d", block_type);
		goto out_err;
	}

	/* find prototype */
	HASH_FIND_STR(*blocklist, type, prot);

	if(prot==NULL) {
		ERR("no component definition named '%s'.", type);
		goto out_err;
	}

	/* check if name is already used */
	HASH_FIND_STR(*blocklist, name, newc);

	if(newc!=NULL) {
		ERR("there already exists a component named '%s'", name);
		goto out_err;
	}

	/* clone */
	if((newc = malloc(sizeof(u5c_block_t)))==NULL)
		goto out_err_nomem;

	memset(newc, 0x0, sizeof(u5c_block_t));

	/* copy attributes of prototype */
	newc->type = prot->type;

	if((newc->name=strdup(name))==NULL) goto out_free_all;
	if((newc->meta_data=strdup(prot->meta_data))==NULL) goto out_free_all;
	if((newc->prototype=strdup(prot->name))==NULL) goto out_free_all;

	/* ports */
	if(prot->ports) {
		for(port_ptr=prot->ports,i=1; port_ptr->name!=NULL; port_ptr++,i++) {
			newc->ports = realloc(newc->ports, sizeof(u5c_port_t) * i);
			if (newc->ports==NULL)
				goto out_free_all;
			if(u5c_clone_port_data(port_ptr, &newc->ports[i-1]) != 0)
				goto out_free_all;
		}
	}

	/* ops */
	newc->init=prot->init;
	newc->start=prot->start;
	newc->stop=prot->stop;
	newc->cleanup=prot->cleanup;

	/* this needs something smarter to work for all block types */
	newc->step=prot->step;

	newc->ports = realloc(newc->ports, sizeof(u5c_port_t) * (i+1));
	memset(&newc->ports[i], 0x0, sizeof(u5c_port_t));

	/* register component */
	if(u5c_block_register(ni, newc))
		goto out_err;

	/* all ok */
	return newc;

 out_free_all:
	if(newc->prototype) free(newc->prototype);
	if(newc->meta_data) free(newc->meta_data);
	if(newc->name) free(newc->name);
	if(newc) free(newc);
 out_err_nomem:
	ERR("insufficient memory");
 out_err:
	return NULL;
}


/**
 * u5c_block_destroy - unregister a block and free its memory.
 *
 * @param ni
 * @param block_type
 * @param name
 *
 * @return
 */
int u5c_block_destroy(u5c_node_info_t *ni, uint32_t block_type, const char* name)
{
	int ret;
	u5c_block_t *c;
	u5c_port_t *port_ptr;

	c = u5c_block_unregister(ni, block_type, name);

	if(c==NULL) {
		ERR("no component named '%s'", name);
		ret = ENOSUCHBLOCK;
		goto out;
	}

	/* ports */
	for(port_ptr=c->ports; port_ptr->name!=NULL; port_ptr++) {
		u5c_port_free_data(port_ptr);
	}

	if(c->ports)
		free(c->ports);

	if(c->prototype) free(c->prototype);
	if(c->meta_data) free(c->meta_data);
	if(c->name) free(c->name);
	if(c) free(c);

	/* all ok */
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
	uint32_t ret;
	const char *tp;
	u5c_block_t *iaptr;

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
		ERR("mismatching types data: %s, port: %s", tp, port->in_type->name);
		goto out;
	}

	/* loop over all in-interactions until data is read */
	for(iaptr=port->in_interaction; iaptr->name!=NULL; iaptr++) {
		if((ret=iaptr->read(iaptr, data)) == PORT_READ_NEWDATA)
			goto out;
	}

 out:
	return ret;
}

/**
 * __port_write - write a sample to a port
 * @param port
 * @param data
 *
 * This function will not check if the type matches.
 */
void __port_write(u5c_port_t* port, u5c_data_t* data)
{
	const char *tp;
	u5c_block_t *iaptr;

	if (!port) {
		ERR("port null");
		/* report error */
		goto out;
	};

	if ((port->attrs & PORT_DIR_OUT) == 0) {
		ERR("not an OUT-port");
		/* report error */
		goto out;
	};

	if(port->out_type != data->type) {
		tp=get_typename(data);
		ERR("mismatching types data: %s, port: %s", tp, port->out_type->name);
		goto out;
	}

	/* iterate over all write-interactions and call their write method */
	for(iaptr=port->out_interaction; iaptr->name!=NULL; iaptr++)
		iaptr->write(iaptr, data);

	port->stat_writes++;
 out:
	return;
}
