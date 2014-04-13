/*
 * microblx: embedded, real-time safe, reflective function blocks.
 * Copyright (C) 2013,2014 Markus Klotzbuecher <markus.klotzbuecher@mech.kuleuven.be>
 *
 * microblx is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 or (at your option)
 * any later version.
 *
 * microblx is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with eCos; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * As a special exception, if other files instantiate templates or use
 * macros or inline functions from this file, or you compile this file
 * and link it with other works to produce a work based on this file,
 * this file does not by itself cause the resulting work to be covered
 * by the GNU General Public License. However the source code for this
 * file must still be made available in accordance with section (3) of
 * the GNU General Public License.
 *
 * This exception does not invalidate any other reasons why a work
 * based on this file might be covered by the GNU General Public
 * License.
*/
#ifndef _UBX_ONCE_H_
#define _UBX_ONCE_H_

#define CONFIG_DUMPABLE		1	/* enable for dumps even when running with priviledges */
/* #define CONFIG_MLOCK_ALL	1 */
#define CONFIG_PARANOIA		1	/* enable extra checks */

#define CONFIG_TYPECHECK_EXTRA	1	/* enable extra typechecking on read/write */

/* constants */
#define NSEC_PER_SEC		1000000000
#define NSEC_PER_USEC           1000

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#ifdef CONFIG_DUMPABLE
#include <sys/prctl.h>
#endif

#include "uthash.h"
#include "ubx_types.h"
#include "ubx_proto.h"

/*
 * Debug stuff
 */

#ifdef DEBUG
# define DBG(fmt, args...) ( fprintf(stderr, "%s: ", __FUNCTION__), \
			     fprintf(stderr, fmt, ##args),	    \
			     fprintf(stderr, "\n") )
#else
# define DBG(fmt, args...)  do {} while (0)
#endif

#define ERR(fmt, args...) ( fprintf(stderr, "ERR %s: ", __FUNCTION__),	\
			    fprintf(stderr, fmt, ##args),		\
			    fprintf(stderr, "\n") )

/* same as ERR but errnum */
#define ERR2(err_num, fmt, args...) ( fprintf(stderr, "ERR %s: ", __FUNCTION__), \
				      fprintf(stderr, fmt, ##args), \
				      fprintf(stderr, ": %s", strerror(err_num)), \
				      fprintf(stderr, "\n") )

#define MSG(fmt, args...) ( fprintf(stderr, "%s: ", __FUNCTION__),	\
			    fprintf(stderr, fmt, ##args),		\
			    fprintf(stderr, "\n") )


#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define UBX_MODULE_LICENSE_SPDX(l) \
__attribute__ ((visibility("default"))) char __ubx_module_license_spdx[] = #l;

/* module init, cleanup */
#define UBX_MODULE_INIT(initfn) \
__attribute__ ((visibility("default"))) int __ubx_initialize_module(ubx_node_info_t* ni) { return initfn(ni); }

#define UBX_MODULE_CLEANUP(exitfn) \
__attribute__ ((visibility("default"))) void __ubx_cleanup_module(ubx_node_info_t* ni) { exitfn(ni); }

/* type definition helpers */
#define def_basic_ctype(typename) { .name=#typename, .type_class=TYPE_CLASS_BASIC, .size=sizeof(typename) }

#define def_struct_type(typename, hexdata) \
{					\
	.name=#typename,		\
	.type_class=TYPE_CLASS_STRUCT,	\
	.size=sizeof(typename),		\
	.private_data=(void*) hexdata,	\
}


/* normally the user would have to box/unbox his value himself. This
 * generate a strongly typed, automatic boxing version for
 * convenience. */
#define def_write_fun(function_name, typename)		\
static void function_name(ubx_port_t* port, typename *outval)	\
{							\
 ubx_data_t val;					\
 if(port==NULL) { ERR("port is NULL"); return; }	\
 checktype(port->block->ni, port->out_type, #typename, port->name, 0);	\
 val.data = outval;					\
 val.type = port->out_type;				\
 val.len=1;						\
 __port_write(port, &val);				\
}							\

/* generate a typed read function: arguments to the function are the
 * port and a pointer to the result value.
 */
#define def_read_fun(function_name, typename)		 \
static int32_t function_name(ubx_port_t* port, typename *inval) \
{							\
 ubx_data_t val;					\
 if(port==NULL) { ERR("port is NULL"); return -1; }	\
 checktype(port->block->ni, port->in_type, #typename, port->name, 1);	\
 val.type=port->in_type;				\
 val.data = inval;					\
 val.len = 1;						\
 return __port_read(port, &val);			\
}							\

/* these ones are for arrays */
#define def_write_arr_fun(function_name, typename, arrlen)	\
static void function_name(ubx_port_t* port, typename (*outval)[arrlen]) \
{							\
 ubx_data_t val;					\
 if(port==NULL) { ERR("port is NULL"); return; }	\
 checktype(port->block->ni, port->out_type, #typename, port->name, 0);	\
 val.data = outval;					\
 val.type = port->out_type;				\
 val.len=arrlen;					\
 __port_write(port, &val);				\
}							\

#define def_read_arr_fun(function_name, typename, arrlen)	 \
static int32_t function_name(ubx_port_t* port, typename (*inval)[arrlen])	\
{							\
 ubx_data_t val;					\
 if(port==NULL) { ERR("port is NULL"); return -1; }	\
 checktype(port->block->ni, port->in_type, #typename, port->name, 1); \
 val.type = port->in_type;				\
 val.data = inval;					\
 val.len = arrlen;					\
 return __port_read(port, &val);			\
}							\


#ifdef __cplusplus
}
#endif

#endif
