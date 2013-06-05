
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


/**
 * u5c_port_free_data - free additional memory used by port.
 *
 * @param p port pointer
 */
static void u5c_port_free_data(u5c_port_t* p)
{
	if(p->out_type_name) free(p->out_type_name);
	if(p->in_type_name) free(p->in_type_name);

	ERR("port %s, p->out_interaction: 0x%lx, p->in_interaction: 0x%lx",
	    p->name, (unsigned long) p->out_interaction, (unsigned long) p->in_interaction);

	if(p->in_interaction) free(p->in_interaction);
	/* if(p->out_interaction) free(p->out_interaction); */

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
 * u5c_block_create - clone a new block from an existing one.
 *
 * @param ni
 * @param block_type
 * @param type
 * @param name
 *
 * @return
 */
u5c_block_t* u5c_block_create(u5c_node_info_t* ni, uint32_t block_type, const char* type, const char* name)
{
	int i;
	u5c_block_t *newc, *prot, **blocklist;
	const u5c_port_t *port_ptr;

	/* retrieve the list corresponding to the block_type */
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

	/* do we have ports? */
	if(prot->ports) {
		for(port_ptr=prot->ports,i=1; port_ptr->name!=NULL; port_ptr++,i++) {
			newc->ports = realloc(newc->ports, sizeof(u5c_port_t) * i);
			if (newc->ports==NULL)
				goto out_free_all;
			if(u5c_clone_port_data(port_ptr, &newc->ports[i-1]) != 0)
				goto out_free_all;
		}

		/* increase size by one for NULL element */
		if((newc->ports = realloc(newc->ports, sizeof(u5c_port_t) * i))==NULL)
			goto out_free_all;

		memset(&newc->ports[i-1], 0x0, sizeof(u5c_port_t));
	}

	/* ops */
	newc->init=prot->init;
	newc->start=prot->start;
	newc->stop=prot->stop;
	newc->cleanup=prot->cleanup;

	/* copy special ops */
	switch(block_type) {
	case BLOCK_TYPE_COMPUTATION:
		newc->step=prot->step;
		break;
	case BLOCK_TYPE_INTERACTION:
		newc->read=prot->read;
		newc->write=prot->write;
		break;
	case BLOCK_TYPE_TRIGGER:
		newc->add=prot->add;
		newc->rm=prot->rm;
		break;
	}

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

int array_block_add(u5c_block_t **arr, u5c_block_t *newblock)
{
	int ret;
	unsigned long newlen; /* new length of array including NULL element */
	u5c_block_t *tmpb;

	/* determine newlen
	 * with one element, the array is size two to hold the terminating NULL element.
	 */
	if(*arr==NULL)
		newlen=2;
	else
		for(tmpb=*arr, newlen=2; tmpb->name!=NULL; tmpb++,newlen++);

	if((*arr=realloc(*arr, sizeof(u5c_block_t*) * newlen))==NULL) {
		ERR("insufficient memory");
		ret=EOUTOFMEM;
		goto out;
	}

	arr[newlen-2]=newblock;
	arr[newlen-1]=NULL;
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
	if(c->ports!=NULL) {
		for(port_ptr=c->ports; port_ptr->name!=NULL; port_ptr++)
			u5c_port_free_data(port_ptr);

		free(c->ports);
	}

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

	/* iterate over all write-interactions and call their write method */
	for(iaptr=port->out_interaction; iaptr->name!=NULL; iaptr++)
		iaptr->write(iaptr, data);

	port->stat_writes++;
 out:
	return;
}
