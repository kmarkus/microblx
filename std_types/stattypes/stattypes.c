/*
 * The basic types
 */

#include <stdint.h>

#include "ubx.h"

#include "types/ubx_tstat.h"
#include "types/ubx_tstat.h.hexarr"

/* declare types */
ubx_type_t types[] = {
	def_struct_type(struct ubx_tstat, &ubx_tstat_h),
	{ NULL },
};

static int stattypes_init(ubx_node_info_t* ni)
{
	DBG(" ");
	ubx_type_t *tptr;
	for(tptr=types; tptr->name!=NULL; tptr++) {
		/* TODO check for errors */
		ubx_type_register(ni, tptr);
	}

	return 0;
}

static void stattypes_cleanup(ubx_node_info_t *ni)
{
	DBG(" ");
	const ubx_type_t *tptr;

	for(tptr=types; tptr->name!=NULL; tptr++)
		ubx_type_unregister(ni, tptr->name);
}

UBX_MODULE_INIT(stattypes_init)
UBX_MODULE_CLEANUP(stattypes_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
