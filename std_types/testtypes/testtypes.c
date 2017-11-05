/*
 * The basic types
 */

#include <stdint.h>

#include "ubx.h"

#include "types/test_trig_conf.h"
#include "types/test_trig_conf.h.hexarr"


/* declare types */
ubx_type_t types[] = {
	def_basic_ctype(char[50]),
	def_struct_type(struct test_trig_conf, &test_trig_conf_h),
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

UBX_MODULE_INIT(testtypes_init)
UBX_MODULE_CLEANUP(testtypes_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
