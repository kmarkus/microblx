
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

void u5c_cleanup(u5c_node_info_t* ni)
{
	/* clean up all entities */
}

int u5c_register_component(u5c_node_info_t *ni, u5c_component_t* comp)
{
	int ret = 0;
	u5c_component_t* tmpc;

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

int u5c_unregister_component(u5c_node_info_t* ni, const char* name)
{
	int ret = 0;
	u5c_component_t* tmpc;

	HASH_FIND_STR(ni->components, name, tmpc);

	if(tmpc==NULL) {
		ERR("no component '%s' registered.", name);
		ret=-1;
		goto out;
	};

	HASH_DEL(ni->components, tmpc);
 out:
	return ret;
}

int u5c_num_components(u5c_node_info_t* ni)
{
	return HASH_COUNT(ni->components);
}

/* create a copy of the given port
 * Function ALLOCATES
 */
int u5c_clone_port_data(const u5c_port_t *psrc, u5c_port_t *pcopy)
{
	DBG("cloning port 0x%lx, '%s'\n", (uint64_t) psrc, psrc->name);

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
	if(pcopy->out_type_name) free(pcopy->out_type_name);
	if(pcopy->in_type_name) free(pcopy->in_type_name);
	if(pcopy->meta_data) free(pcopy->meta_data);
	if(pcopy->name) free(pcopy->name);
 out_err:
 	return -1;
}

/* clone component
 * Function ALLOCATES
 */
u5c_component_t* u5c_create_component(u5c_node_info_t* ni, const char* type, const char* name)
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

	/* clone */
	newc = malloc(sizeof(u5c_component_t));
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

	newc->ports = realloc(newc->ports, sizeof(u5c_port_t) * (i+1));
	memset(&newc->ports[i], 0x0, sizeof(u5c_port_t));

	/* all ok */
	return newc;

 out_free_all:
	if(newc->prototype) free(newc->prototype);
	if(newc->meta_data) free(newc->meta_data);
	if(newc->name) free(newc->name);
	if(newc) free(newc);
 out_err:
	ERR("insufficient memory");
	return NULL;
}

/* lowlevel port read */
uint32_t __port_read(u5c_port_t* port, u5c_data_t* res)
{
	uint32_t ret = PORT_READ_NODATA;

	if (!port) {
		ERR("port null");
		ret = PORT_INVALID;
		goto out;
	};

	if ((port->attrs & PORT_DIR_IN) == 0) {
		ERR("not an IN-port");
		ret = PORT_INVALID_TYPE;
		goto out;
	};

	if (!port->in_interaction)
		goto out;

	/* this is madness */
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
