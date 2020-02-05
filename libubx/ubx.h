/*
 * microblx: embedded, realtime safe, reflective function blocks.
 *
 * Copyright (C) 2013,2014 Markus Klotzbuecher
 *                         <markus.klotzbuecher@mech.kuleuven.be>
 * Copyright (C) 2014-2018 Markus Klotzbuecher <mk@mkio.de>
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef _UBX_H
#define _UBX_H

/* enable for dumps even when running with priviledges */
#define CONFIG_DUMPABLE		1
/* #define CONFIG_MLOCK_ALL	1 */
/* enable extra checks */
#define CONFIG_PARANOIA		1
/* enable extra typechecking on read/write */
#define CONFIG_TYPECHECK_EXTRA	1

/* constants */
#define NSEC_PER_SEC		1000000000
#define NSEC_PER_USEC           1000

/* config array length checking */
#define CONFIG_LEN_MAX		UINT16_MAX

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
#include <limits.h>

#ifdef CONFIG_DUMPABLE
#include <sys/prctl.h>
#endif

#include <uthash.h>

#ifdef __cplusplus
/* ubx_typescpp includes ubx_types */
/* this is needed since no ifdef protection is allowed in ubx_types. */
#include "ubx_types_cpp.h"
#else
#include "ubx_types.h"
#endif

#include "ubx_proto.h"
#include "rtlog.h"

/*
 * Debug stuff
 */

#ifdef DEBUG
# define DBG(fmt, args...) (fprintf(stderr, "%s: ", __func__),	\
			    fprintf(stderr, fmt, ##args),	\
			    fprintf(stderr, "\n"))
#else
# define DBG(fmt, args...)  do {} while (0)
#endif

#define ERR(fmt, args...) (fprintf(stderr, "ERR %s: ", __func__),	\
			   fprintf(stderr, fmt, ##args),		\
			   fprintf(stderr, "\n"))

/* same as ERR but errnum */
#define ERR2(err_num, fmt, args...) (fprintf(stderr, "ERR %s: ", __func__),		\
				     fprintf(stderr, fmt, ##args),			\
				     fprintf(stderr, ": %s", strerror(err_num)),	\
				     fprintf(stderr, "\n"))

#define MSG(fmt, args...) (fprintf(stderr, "%s: ", __func__),	\
			   fprintf(stderr, fmt, ##args),	\
			   fprintf(stderr, "\n"))

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define _QUOTE(x) #x
#define QUOTE(x) _QUOTE(x)

#define UBX_MODULE_LICENSE_SPDX(l) \
__attribute__ ((visibility("default"))) char __ubx_module_license_spdx[] = QUOTE(l);

/* module init, cleanup */
#define UBX_MODULE_INIT(initfn) \
__attribute__ ((visibility("default"))) int __ubx_initialize_module(ubx_node_info_t *ni) { return initfn(ni); }

#define UBX_MODULE_CLEANUP(exitfn) \
__attribute__ ((visibility("default"))) void __ubx_cleanup_module(ubx_node_info_t *ni) { exitfn(ni); }

/* type definition helpers */
#define def_basic_ctype(typename) { .name = #typename, .type_class = TYPE_CLASS_BASIC, .size = sizeof(typename) }

#define def_struct_type(typename, hexdata)	\
{						\
	.name = QUOTE(typename),		\
	.type_class = TYPE_CLASS_STRUCT,	\
	.size = sizeof(typename),		\
	.private_data = (void *)hexdata,	\
}

int checktype(ubx_node_info_t *ni, ubx_type_t *required, const char *tcheck_str, const char *portname, int isrd);

/*
 * normally the user would have to box/unbox his value himself. This
 * generates a strongly typed, automatic boxing version for
 * convenience.
 */
#define def_write_fun(function_name, typename)						\
static void function_name(ubx_port_t *port, typename *outval)				\
{											\
	ubx_data_t val;									\
											\
	if (port == NULL) {								\
		ubx_err(port->block, "%s: port is NULL", __func__);			\
		return;									\
	}										\
	checktype(port->block->ni, port->out_type, QUOTE(typename), port->name, 0);	\
	val.data = outval;								\
	val.type = port->out_type;							\
	val.len = 1;									\
	__port_write(port, &val);							\
}

/*
 * generate a typed read function: arguments to the function are the
 * port and a pointer to the result value.
 */
#define def_read_fun(function_name, typename)						\
static int32_t function_name(ubx_port_t *port, typename *inval)				\
{											\
	ubx_data_t val;									\
											\
	if (port == NULL) {								\
		ERR("%s: port is NULL", __func__);					\
		return EINVALID_PORT;							\
	}										\
	checktype(port->block->ni, port->in_type, QUOTE(typename), port->name, 1);	\
	val.type = port->in_type;							\
	val.data = inval;								\
	val.len = 1;									\
	return __port_read(port, &val);							\
}

/* these ones are for arrays */
#define def_write_arr_fun(function_name, typename, arrlen)				\
static void function_name(ubx_port_t *port, typename(*outval)[arrlen])			\
{											\
	ubx_data_t val;									\
											\
	if (port == NULL) {								\
		ERR("%s: port is NULL", __func__);					\
		return;									\
	}										\
	checktype(port->block->ni, port->out_type, QUOTE(typename), port->name, 0);	\
	val.data = outval;								\
	val.type = port->out_type;							\
	val.len = arrlen;								\
	__port_write(port, &val);							\
}

#define def_read_arr_fun(function_name, typename, arrlen)				\
static int32_t function_name(ubx_port_t *port, typename(*inval)[arrlen])		\
{											\
	ubx_data_t val;									\
											\
	if (port == NULL) {								\
		ERR("%s: port is NULL", __func__);					\
		return EINVALID_PORT;							\
	}										\
											\
	checktype(port->block->ni, port->in_type, QUOTE(typename), port->name, 1);	\
	val.type = port->in_type;							\
	val.data = inval;								\
	val.len = arrlen;								\
	return __port_read(port, &val);							\
}

/**
 * define a helper function for reading	an array of dynamic length
 * @param port port to read from
 * @param inval ptr to data structure into which to store the read value
 * @param len array length of inval buffer
 * @return actual length read
 */
#define def_read_dynarr_fun(function_name, typename)					\
static int32_t function_name(ubx_port_t *port, typename *inval, long len) 		\
{											\
	ubx_data_t val;									\
											\
	if (port == NULL) {								\
		ERR("%s: port is NULL", __func__);					\
		return EINVALID_PORT;							\
	}										\
											\
	checktype(port->block->ni, port->in_type, QUOTE(typename), port->name, 1);	\
	val.type = port->in_type;							\
	val.data = inval;								\
	val.len = len;									\
	return __port_read(port, &val);							\
}

/**
 * define a helper function for writing	an array of dynamic length
 * @param port port to write to
 * @param outval ptr to data structure into which to store the read value
 * @param len array length of outval buffer
 */
#define def_write_dynarr_fun(function_name, typename)					\
static int32_t function_name(ubx_port_t *port, typename *outval, long len) 		\
{											\
	ubx_data_t val;									\
											\
	if (port == NULL) {								\
		ERR("%s: port is NULL", __func__);					\
		return EINVALID_PORT;							\
	}										\
	checktype(port->block->ni, port->out_type, QUOTE(typename), port->name, 0);	\
	val.data = outval;								\
	val.type = port->out_type;							\
	val.len = len;									\
	__port_write(port, &val);							\
	return 0;							                \
}

#define def_cfg_getptr_fun(function_name, typename)					\
long function_name(ubx_block_t *b, const char *cfg_name, const typename **valptr)	\
{											\
	long ret = -1;									\
	ubx_config_t *c;								\
	ubx_type_t *t;									\
											\
	c = ubx_config_get(b, cfg_name);						\
	if (c == NULL)									\
		goto out;								\
											\
	t = ubx_type_get(b->ni, QUOTE(typename));					\
	if (t == NULL) {								\
		ubx_err(b, "unknown type %s", QUOTE(typename));				\
		goto out;								\
	}										\
											\
	if (t != c->type) {								\
		ubx_err(b, "%s: mismatch: expected %s but config %s is of type %s",	\
			__func__, t->name, cfg_name, c->type->name);			\
		goto out;								\
	}										\
											\
	ret = ubx_config_get_data_ptr(b, cfg_name, (void **)valptr);			\
out:											\
	return ret;									\
}											\

#ifdef __cplusplus
}
#endif

#endif /* _UBX_H */
