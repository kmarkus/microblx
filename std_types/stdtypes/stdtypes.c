/*
 * The basic types
 */

#include <stdint.h>

#include "ubx.h"

#include "types/tstat.h"
#include "types/tstat.h.hexarr"

#include "types/trig_spec.h"
#include "types/trig_spec.h.hexarr"

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

	/* std struct types */
	def_struct_type(struct ubx_tstat, &tstat_h),
	def_struct_type(struct ubx_trig_spec, &trig_spec_h),
};

static int stdtypes_init(ubx_node_t* nd)
{
	for(int i=0; i<ARRAY_SIZE(basic_types); i++)
		ubx_type_register(nd, &basic_types[i]);

	return 0;
}

static void stdtypes_cleanup(ubx_node_t *nd)
{
	for(int i=0; i<ARRAY_SIZE(basic_types); i++)
		ubx_type_unregister(nd, basic_types[i].name);
}

UBX_MODULE_INIT(stdtypes_init)
UBX_MODULE_CLEANUP(stdtypes_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
