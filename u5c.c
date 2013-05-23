#include <sys/mman.h>

#include "u5c.h"

int u5c_initialize(u5c_node_info* ni)
{
	if(mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
		ERR2(errno, "");
		goto out_err;
	};

	ni->components=NULL; components_len=0;
	ni->interactions=NULL; interactions_len=0;
	ni->types=NULL; types_len=0;

 out:
	return 0;

 out_err:
	return -1;
}

void u5c_cleanup(u5c_node_info* ni)
{
	/* */
}

int u5c_register_component(u5c_node_info *ni, u5c_component* comp)
{
	/* sarray = (structName **) realloc(sarray, (sarray_len + offset) * sizeof(structName *)); */
	ni->components = (u5c_component**) realloc(ni->components, ++ni->components_len * sizeof(u5c_component));
	ni->components[components_len-1]=comp;
}

/* lowlevel port read */
uint32_t __port_read(u5c_port_t* port, u5c_type_t* res)
{
	uint32_t ret = PORT_READ_NODATA;

	if (!port) {
		ERR("port null");
		ret = PORT_INVALID;
		goto out;
	};

	if (port->type && PORT_DIR_OUT) {
		ERR("can't read out port");
		ret = PORT_INVALID_TYPE;
		goto out;
	};

	if (!port->read || !port->interaction)
		goto out;

	ret = port->read(port->interaction)
		
 out:
	return ret;
}

void u5c_unregister_component(u5c_node_info *ni, u5c_component* comp);
int u5c_register_interaction(u5c_node_info *ni, u5c_interaction* inter);
void u5c_unregister_interaction(u5c_node_info *ni, u5c_interaction* inter);
int u5c_register_type(u5c_node_info *ni, u5c_type* type);
void u5c_unregister_type(u5c_node_info *ni, u5c_type* type);

int u5c_create_component(char *type);
int u5c_destroy_component(char *name);
int u5c_connect(u5c_component* comp1, u5c_component* comp1, u5c_interaction* ia)
int u5c_disconnect(u5c_component* comp1, u5c_component* comp1, u5c_interaction* ia)
