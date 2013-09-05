/*
 * The basic types
 */

#include <stdint.h>

#include "ubx.h"

#include "types/kdl_vector.h"
#include "types/kdl_vector.h.hexarr"

#include "types/kdl_rotation.h"
#include "types/kdl_rotation.h.hexarr"

#include "types/kdl_frame.h"
#include "types/kdl_frame.h.hexarr"

#include "types/kdl_twist.h"
#include "types/kdl_twist.h.hexarr"

#include "types/kdl_wrench.h"
#include "types/kdl_wrench.h.hexarr"


/* declare types */
ubx_type_t types[] = {
	def_struct_type(struct kdl_vector, &kdl_vector_h),
	def_struct_type(struct kdl_rotation, &kdl_rotation_h),
	def_struct_type(struct kdl_frame, &kdl_frame_h),
	def_struct_type(struct kdl_twist, &kdl_twist_h),
	def_struct_type(struct kdl_wrench, &kdl_wrench_h),
	{ NULL },
};

static int testtypes_init(ubx_node_info_t* ni)
{
	DBG(" ");
	ubx_type_t *tptr;
	for(tptr=types; tptr->name!=NULL; tptr++) {
		/* TODO check for errors */
		ubx_type_register(ni, tptr);
	}

	return 0;
}

static void testtypes_cleanup(ubx_node_info_t *ni)
{
	DBG(" ");
	const ubx_type_t *tptr;

	for(tptr=types; tptr->name!=NULL; tptr++)
		ubx_type_unregister(ni, tptr->name);
}

module_init(testtypes_init)
module_cleanup(testtypes_cleanup)
