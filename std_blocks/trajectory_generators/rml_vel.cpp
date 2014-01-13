#include "rml_vel.hpp"

#include <ReflexxesAPI.h>
#include <RMLPositionFlags.h>
#include <RMLPositionInputParameters.h>
#include <RMLPositionOutputParameters.h>

/* define a structure for holding the block local state. By assigning an
 * instance of this struct to the block private_data pointer (see init), this
 * information becomes accessible within the hook functions.
 */
struct rml_vel_info
{
	ReflexxesAPI *RML;
	RMLPositionInputParameters *IP;
	RMLPositionOutputParameters *OP;
	RMLPositionFlags Flags;

	/* this is to have fast access to ports for reading and writing, without
	 * needing a hash table lookup */
	struct rml_vel_port_cache ports;
};

/* init */
int rml_vel_init(ubx_block_t *b)
{
	int ret = -1;
	struct rml_vel_info *inf;

	/* allocate memory for the block local state */
	if ((inf = (rml_vel_info*) calloc(1, sizeof(struct rml_vel_info)))==NULL) {
		ERR("rml_vel: failed to alloc memory");
		ret=EOUTOFMEM;
		goto out;
	}
	b->private_data=inf;
	update_port_cache(b, &inf->ports);
	ret=0;
out:
	return ret;
}

/* start */
int rml_vel_start(ubx_block_t *b)
{
	/* struct _info *inf = (struct _info*) b->private_data; */
	int ret = 0;
	return ret;
}

/* stop */
void rml_vel_stop(ubx_block_t *b)
{
	/* struct _info *inf = (struct _info*) b->private_data; */
}

/* cleanup */
void rml_vel_cleanup(ubx_block_t *b)
{
	free(b->private_data);
}

/* step */
void rml_vel_step(ubx_block_t *b)
{


	/*
	struct rml_vel_info *inf = (struct rml_vel_info*) b->private_data;


	*/

}

