/*
 * The basic types
 */

#define DEBUG 1

#include <stdint.h>

#include "u5c.h"

#define def_basic_ctype(typename) { .name=#typename, .class=TYPE_CLASS_BASIC, .size=sizeof(typename) }

/* declare types */
const u5c_type_t basic_types[] = {
	/* basic types */
	def_basic_ctype(char),	     def_basic_ctype(unsigned char),	   def_basic_ctype(signed char),
	def_basic_ctype(short),      def_basic_ctype(unsigned short),	   def_basic_ctype(signed short),
	def_basic_ctype(int), 	     def_basic_ctype(unsigned int),	   def_basic_ctype(signed int),
	def_basic_ctype(long),       def_basic_ctype(unsigned long),	   def_basic_ctype(signed long),
	def_basic_ctype(long long),  def_basic_ctype(unsigned long long),  def_basic_ctype(signed long long),

	/* floating point */
	def_basic_ctype(float),      def_basic_ctype(double),

	/* stdint types */
	def_basic_ctype(int8_t),  def_basic_ctype(int16_t),  def_basic_ctype(int32_t),  def_basic_ctype(int64_t),
	def_basic_ctype(uint8_t), def_basic_ctype(uint16_t), def_basic_ctype(uint32_t),	def_basic_ctype(uint64_t),

	{ NULL },
};

static int stdtypes_init(u5c_node_info_t* ni)
{
	DBG(" ");	
	const u5c_type_t *tptr;
	for(tptr=basic_types; tptr->name!=NULL; tptr++) {
		/* TODO check for errors */
		u5c_type_register(ni, tptr);
	}

	return 0;
}

static void stdtypes_cleanup(u5c_node_info_t *ni)
{
	DBG(" ");
	const u5c_type_t *tptr;

	for(tptr=basic_types; tptr->name!=NULL; tptr++)
		u5c_type_unregister(ni, tptr->name);
}

module_init(stdtypes_init)
module_cleanup(stdtypes_cleanup)
