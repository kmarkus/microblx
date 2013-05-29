
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

/* clone component */
int u5c_create_component(u5c_node_info_t* ni, const char* type, const char* name)
{
	int ret = -1;
	/* find prototype */
	u5c_component_t *newc, *prot;

	HASH_FIND_STR(ni->components, type, prot);

	if(prot==NULL) {
		ERR("no component definition named '%s'.", type);
		goto out;
	}

	/* clone */
	newc = malloc(sizeof(u5c_component_t));
	memcpy(newc, prot, sizeof(u5c_component_t));
	newc->name=strdup(name);

	/* to be continued */

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
