/*
 * The basic types
 */

#include <stdint.h>

#include "ubx.h"

#include "types/vector.h"
#include "types/vector.h.hexarr"

#include "types/rotation.h"
#include "types/rotation.h.hexarr"

#include "types/frame.h"
#include "types/frame.h.hexarr"

#include "types/twist.h"
#include "types/twist.h.hexarr"

#include "types/wrench.h"
#include "types/wrench.h.hexarr"


/* declare types */
ubx_type_t types[] = {
	def_struct_type("kdl", struct Vector, &vector_h),
	def_struct_type("kdl", struct Rotation, &rotation_h),
	def_struct_type("kdl", struct Frame, &frame_h),
	def_struct_type("kdl", struct Twist, &twist_h),
	def_struct_type("kdl", struct Wrench, &wrench_h),
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
