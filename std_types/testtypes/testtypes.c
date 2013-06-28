/*
 * The basic types
 */

#define DEBUG 1

#include <stdint.h>

#include "u5c.h"

#include "types/vector.h"
#include "types/vector.h.hexarr"
#include "types/rotation.h"
#include "types/rotation.h.hexarr"
#include "types/frame.h"
#include "types/frame.h.hexarr"

#define def_struct_type(module, typename, hexdata) \
{ 					\
	.name=module "/" #typename, 	\
	.type_class=TYPE_CLASS_STRUCT,	\
	.size=sizeof(typename),		\
	.private_data=hexdata, 		\
}

/* declare types */
u5c_type_t types[] = {
	def_struct_type("testtypes", struct Vector, &vector_h),
	def_struct_type("testtypes", struct Rotation, &rotation_h),
	def_struct_type("testtypes", struct Frame, &frame_h),
	{ NULL },
};

static int testtypes_init(u5c_node_info_t* ni)
{
	DBG(" ");
	u5c_type_t *tptr;
	for(tptr=types; tptr->name!=NULL; tptr++) {
		/* TODO check for errors */
		u5c_type_register(ni, tptr);
	}

	return 0;
}

static void testtypes_cleanup(u5c_node_info_t *ni)
{
	DBG(" ");
	const u5c_type_t *tptr;

	for(tptr=types; tptr->name!=NULL; tptr++)
		u5c_type_unregister(ni, tptr->name);
}

module_init(testtypes_init)
module_cleanup(testtypes_cleanup)
