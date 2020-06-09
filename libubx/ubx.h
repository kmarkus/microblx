/*
 * microblx: embedded, realtime safe, reflective function blocks.
 *
 * Copyright (C) 2013,2014 Markus Klotzbuecher
 *                         <markus.klotzbuecher@mech.kuleuven.be>
 * Copyright (C) 2014-2020 Markus Klotzbuecher <mk@mkio.de>
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef _UBX_H
#define _UBX_H

/* Enable this to configure microblx suitable for being run with
 * valgrind. Right now this will pass RTLD_NODELETE to dlopen to allow
 * it to print meaningful leak traces */
#undef UBX_CONFIG_VALGRIND

#include "typemacros.h"

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
#include <sys/prctl.h>
#include <uthash.h>
#include <utlist.h>

#include "ubx_types.h"

struct ubx_proto_block;
struct ubx_proto_port;
struct ubx_proto_config;

#include "ubx_proto.h"
#include "rtlog.h"


/* constants */
#define NSEC_PER_SEC		1000000000
#define USEC_PER_SEC		1000000
#define NSEC_PER_USEC           1000

/* config array length checking */
#define CONFIG_LEN_MAX		UINT16_MAX

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/* predicates */
#define IS_PROTO(b) (b->prototype == NULL)
#define IS_INSTANCE(b) (b->prototype != NULL)

#define IS_OUTPORT(p) (p->out_type != NULL)
#define IS_INPORT(p)  (p->in_type != NULL)
#define IS_INOUTPORT(p) (IS_INPORT(p) && IS_OUTPORT(b))

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

/* prototype definition */

/**
 * struct ubx_proto_port
 * @doc: docstring
 * @name: name of prototype (i.e. it's type)
 * @attrs: port attributes (currently unused)
 * @out_type_name: name of output type
 * @in_type_name: name of input type
 * @in_data_len: array size of input data
 * @out_data_len: array size output data
 */
typedef struct ubx_proto_port {
	const char *name;
	uint32_t attrs;
	const char *out_type_name;
	long out_data_len;
	const char *in_type_name;
	long in_data_len;
	const char *doc;
} ubx_proto_port_t;

typedef struct ubx_proto_config {
	const char *name;
	const char *type_name;
	uint32_t attrs;
	uint16_t min;
	uint16_t max;
	const char *doc;
} ubx_proto_config_t;

typedef struct ubx_proto_block {
	const char *name;
	const char *meta_data;
	uint32_t attrs;
	uint16_t type;

	struct ubx_proto_port *ports;
	struct ubx_proto_config *configs;

	int (*init)(struct ubx_block *b);
	int (*start)(struct ubx_block *b);
	void (*stop)(struct ubx_block *b);
	void (*cleanup)(struct ubx_block *b);

	union {
		/* COMP_TYPE_COMPUTATION */
		struct {
			void (*step)(struct ubx_block *cblock);
			unsigned long stat_num_steps;
		};

		/* COMP_TYPE_INTERACTION */
		struct {
			long (*read)(struct ubx_block *iblock,
				     ubx_data_t *value);
			void (*write)(struct ubx_block *iblock,
				      const ubx_data_t *value);
			unsigned long stat_num_reads;
			unsigned long stat_num_writes;
		};
	};
} ubx_proto_block_t;

#ifdef __cplusplus
}
#endif

#endif /* _UBX_H */
