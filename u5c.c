
#define DEBUG 1
#include "u5c.h"

int u5c_node_init(u5c_node_info_t* ni)
{
	/* if(mlockall(MCL_CURRENT | MCL_FUTURE) != 0) { */
	/* 	ERR2(errno, " "); */
	/* 	goto out_err; */
	/* }; */

	ni->components=NULL;
	ni->interactions=NULL;
	ni->triggers=NULL;
	ni->types=NULL;

	return 0;

	/* out_err: */
	/* return -1; */
}

void u5c_node_cleanup(u5c_node_info_t* ni)
{
	/* clean up all entities */
}

int u5c_computation_register(u5c_node_info_t *ni, u5c_component_t* comp)
{
	int ret = 0;
	u5c_component_t* tmpc;

	if(comp->type != BLOCK_TYPE_COMPUTATION)
		ret=EINVALID_BLOCK_TYPE;

	HASH_FIND_STR(ni->components, comp->name, tmpc);

	if(tmpc!=NULL) {
		ERR("component '%s' already registered.", comp->name);
		ret=-1;
		goto out;
	};

	HASH_ADD_KEYPTR(hh, ni->components, comp->name, strlen(comp->name), comp);

 out:
	return ret;
}

u5c_component_t* u5c_computation_unregister(u5c_node_info_t* ni, const char* name)
{
	u5c_component_t* tmpc;

	HASH_FIND_STR(ni->components, name, tmpc);

	if(tmpc==NULL) {
		ERR("no component '%s' registered.", name);
		goto out;
	};

	HASH_DEL(ni->components, tmpc);
 out:
	return tmpc;
}

int u5c_num_components(u5c_node_info_t* ni)
{
	return HASH_COUNT(ni->components);
}

void u5c_port_free_data(u5c_port_t* p)
{
	if(p->out_type_name) free(p->out_type_name);
	if(p->in_type_name) free(p->in_type_name);
	if(p->meta_data) free(p->meta_data);
	if(p->name) free(p->name);
}

/* create a copy of the given port
 * Function ALLOCATES
 */
int u5c_clone_port_data(const u5c_port_t *psrc, u5c_port_t *pcopy)
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
u5c_component_t* u5c_computation_create(u5c_node_info_t* ni, const char* type, const char* name)
{
	int i;
	u5c_component_t *newc, *prot;
	const u5c_port_t *port_ptr;

	/* find prototype */
	HASH_FIND_STR(ni->components, type, prot);

	if(prot==NULL) {
		ERR("no component definition named '%s'.", type);
		goto out_err;
	}

	/* check if name is already used */
	HASH_FIND_STR(ni->components, name, newc);
	
	if(newc!=NULL) {
		ERR("there already exists a component named '%s'", name);
		goto out_err;
	}

	/* clone */
	if((newc = malloc(sizeof(u5c_component_t)))==NULL)
		goto out_err_nomem;

	memset(newc, 0x0, sizeof(u5c_component_t));
	
	if((newc->name=strdup(name))==NULL) goto out_free_all;
	if((newc->meta_data=strdup(prot->meta_data))==NULL) goto out_free_all;
	if((newc->prototype=strdup(prot->name))==NULL) goto out_free_all;

	/* ports */
	for(port_ptr=prot->ports,i=1; port_ptr->name!=NULL; port_ptr++,i++) {
		newc->ports = realloc(newc->ports, sizeof(u5c_port_t) * i);
		if (newc->ports==NULL)
			goto out_free_all;
		if(u5c_clone_port_data(port_ptr, &newc->ports[i-1]) != 0)
			goto out_free_all;
	}

	/* ops */
	newc->init=prot->init;
	newc->start=prot->start;
	newc->stop=prot->stop;
	newc->cleanup=prot->cleanup;
	newc->step=prot->step;

	newc->ports = realloc(newc->ports, sizeof(u5c_port_t) * (i+1));
	memset(&newc->ports[i], 0x0, sizeof(u5c_port_t));

	/* register component */
	if(u5c_computation_register(ni, newc))
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

int u5c_component_destroy(u5c_node_info_t *ni, char* name)
{
	int ret;
	u5c_component_t *c;
	u5c_port_t *port_ptr;

	c = u5c_computation_unregister(ni, name);
	
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

/* lowlevel port read */
uint32_t __port_read(u5c_port_t* port, u5c_data_t* res)
{
	uint32_t ret = PORT_READ_NODATA;

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

	if (!port->in_interaction)
		goto out;

	ret = port->in_interaction->read(port->in_interaction, res);
		
 out:
	return ret;
}


void __port_write(u5c_port_t* port, u5c_data_t* res)
{
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

	/* iterate over all write-interactions and call their write method */
 out:
	return;
}
