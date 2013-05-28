
#include "u5c.h"

int u5c_initialize(u5c_node_info_t* ni)
{
	if(mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
		ERR2(errno, " ");
		goto out_err;
	};

	ni->cdefs=NULL;
	ni->cdefs_len=0;

	return 0;

 out_err:
	return -1;
}

void u5c_cleanup(u5c_node_info_t* ni)
{
	/* */
}

int u5c_register_component(u5c_node_info_t *ni, u5c_component_t* comp)
{
	ni->cdefs = (u5c_component_t**) realloc(ni->cdefs, ++ni->cdefs_len * sizeof(u5c_component_t*));
	ni->cdefs[ni->cdefs_len-1]=comp;
	return 0;
}

int u5c_unregister_component(u5c_node_info_t* ni, u5c_component_t* comp)
{
	int ret=0;

	if(ni->cdefs_len<=0) {
		ERR("no components registered");
		ret=-1;
		goto out;
	};

	/* copy the entry into the one to be removed */
	if(ni->cdefs_len>=2) {
		int i;
		for(i=0; i<ni->cdefs_len; i++)
			if(ni->cdefs[i] == comp)
				break;
		ni->cdefs[i] = ni->cdefs[ni->cdefs_len-1];
	}

	/* if cdefs_len=1 the array simple is destructed */
	

	/* shrink by one */
	ni->cdefs = (u5c_component_t**) realloc(ni->cdefs, --ni->cdefs_len * sizeof(u5c_component_t*));
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
