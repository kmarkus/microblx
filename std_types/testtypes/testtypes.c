/*
 * The basic types
 */

#define DEBUG 1

#include <stdint.h>

#include "ubx.h"

#include "types/vector.h"
#include "types/vector.h.hexarr"
#include "types/rotation.h"
#include "types/rotation.h.hexarr"
#include "types/frame.h"
#include "types/frame.h.hexarr"
#include "types/test_trig_conf.h"
#include "types/test_trig_conf.h.hexarr"


/* declare types */
ubx_type_t types[] = {
	def_basic_ctype(char[50]),
	def_struct_type("testtypes", struct Vector, &vector_h),
	def_struct_type("testtypes", struct Rotation, &rotation_h),
	def_struct_type("testtypes", struct Frame, &frame_h),
	def_struct_type("testtypes", struct test_trig_conf, &test_trig_conf_h),
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
