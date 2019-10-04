/*
 * The basic types
 */

#include <stdint.h>

#include "ubx.h"

#include "types/test_trig_conf.h"
#include "types/test_trig_conf.h.hexarr"

#include "types/kdl_vector.h"
#include "types/kdl_vector.h.hexarr"

#include "types/kdl_rotation.h"
#include "types/kdl_rotation.h.hexarr"

#include "types/kdl_frame.h"
#include "types/kdl_frame.h.hexarr"


/* declare types */
ubx_type_t types[] = {
	def_basic_ctype(char[50]),
	def_struct_type(struct test_trig_conf, &test_trig_conf_h),
	def_struct_type(struct kdl_vector, &kdl_vector_h),
	def_struct_type(struct kdl_rotation, &kdl_rotation_h),
	def_struct_type(struct kdl_frame, &kdl_frame_h),
	{ NULL },
};

static int testtypes_init(ubx_node_info_t* ni)
{
	ubx_type_t *tptr;
	for(tptr=types; tptr->name!=NULL; tptr++) {
		/* TODO check for errors */
		ubx_type_register(ni, tptr);
	}

	return 0;
}

static void testtypes_cleanup(ubx_node_info_t *ni)
{
	const ubx_type_t *tptr;

	for(tptr=types; tptr->name!=NULL; tptr++)
		ubx_type_unregister(ni, tptr->name);
}

UBX_MODULE_INIT(testtypes_init)
UBX_MODULE_CLEANUP(testtypes_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
