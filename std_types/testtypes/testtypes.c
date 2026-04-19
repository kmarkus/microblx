/*
 * The basic types
 */

#include "ubx.h"

#include "types/test_trig_conf.h"
#include "types/test_trig_conf.h.hexarr"

#include "types/kdl_vector.h"
#include "types/kdl_vector.h.hexarr"

#include "types/kdl_rotation.h"
#include "types/kdl_rotation.h.hexarr"

#include "types/kdl_frame.h"
#include "types/kdl_frame.h.hexarr"

#include "types/test_with_enum.h"
#include "types/test_with_enum.h.hexarr"

#include "types/test_with_union.h"
#include "types/test_with_union.h.hexarr"

#include "types/test_with_anon_union.h"
#include "types/test_with_anon_union.h.hexarr"

#include "types/test_with_anon_enum.h"
#include "types/test_with_anon_enum.h.hexarr"


/* declare types */
ubx_type_t types[] = {
	def_basic_ctype(char[50]),
	def_struct_type(struct test_trig_conf, &test_trig_conf_h),
	def_struct_type(struct kdl_vector, &kdl_vector_h),
	def_struct_type(struct kdl_rotation, &kdl_rotation_h),
	def_struct_type(struct kdl_frame, &kdl_frame_h),
	def_struct_type(struct test_with_enum, &test_with_enum_h),
	def_struct_type(struct test_with_union, &test_with_union_h),
	def_struct_type(struct test_with_anon_union, &test_with_anon_union_h),
	def_struct_type(struct test_with_anon_enum, &test_with_anon_enum_h),
};

static int testtypes_init(ubx_node_t* nd)
{
	for (unsigned int i=0; i<ARRAY_SIZE(types); i++)
		ubx_type_register(nd, &types[i]);

	return 0;
}

static void testtypes_cleanup(ubx_node_t *nd)
{
	for (unsigned int i=0; i<ARRAY_SIZE(types); i++)
		ubx_type_unregister(nd, types[i].name);
}

UBX_MODULE_INIT(testtypes_init)
UBX_MODULE_CLEANUP(testtypes_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
