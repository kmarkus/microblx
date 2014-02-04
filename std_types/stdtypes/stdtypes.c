/*
 * The basic types
 */

#include <stdint.h>

#include "ubx.h"

/* declare types */
ubx_type_t basic_types[] = {
	/* basic types */
	def_basic_ctype(char),	     def_basic_ctype(unsigned char),	   def_basic_ctype(signed char),
	def_basic_ctype(short),      def_basic_ctype(unsigned short),	   def_basic_ctype(signed short),
	def_basic_ctype(int), 	     def_basic_ctype(unsigned int),	   def_basic_ctype(signed int),
	def_basic_ctype(long),       def_basic_ctype(unsigned long),	   def_basic_ctype(signed long),
	def_basic_ctype(long long),  def_basic_ctype(unsigned long long),  def_basic_ctype(signed long long),

	/* floating point */
	def_basic_ctype(float),      def_basic_ctype(double),
	
	def_basic_ctype(size_t),

	/* stdint types */
	def_basic_ctype(int8_t),  def_basic_ctype(int16_t),  def_basic_ctype(int32_t),  def_basic_ctype(int64_t),
	def_basic_ctype(uint8_t), def_basic_ctype(uint16_t), def_basic_ctype(uint32_t),	def_basic_ctype(uint64_t),

	{ NULL },
};

static int stdtypes_init(ubx_node_info_t* ni)
{
	DBG(" ");	
	ubx_type_t *tptr;
	for(tptr=basic_types; tptr->name!=NULL; tptr++) {
		/* TODO check for errors */
		ubx_type_register(ni, tptr);
	}

	return 0;
}

static void stdtypes_cleanup(ubx_node_info_t *ni)
{
	DBG(" ");
	const ubx_type_t *tptr;

	for(tptr=basic_types; tptr->name!=NULL; tptr++)
		ubx_type_unregister(ni, tptr->name);
}

UBX_MODULE_INIT(stdtypes_init)
UBX_MODULE_CLEANUP(stdtypes_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
