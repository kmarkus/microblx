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

/* constants */
#define NSEC_PER_SEC		1000000000
#define NSEC_PER_USEC           1000

/* config array length checking */
#define CONFIG_LEN_MAX		UINT16_MAX

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

#ifdef __cplusplus
}
#endif

#endif /* _UBX_H */
