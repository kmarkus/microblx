/*
 * RCU wrappers for hot connect/disconnect
 *
 * With ENABLE_URCU these map to liburcu-bp, otherwise to plain loads
 * and stores, i.e. connection changes are only safe while no thread
 * accesses the ports (as before).
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef UBX_RCU_H
#define UBX_RCU_H

#include <config.h>

#ifdef UBX_URCU

/* inline read lock/unlock: as library calls they cost ~4x per port
 * access. This compiles LGPL-2.1 code into libubx (MPL-2.0 allows
 * that combination) and ties it to the liburcu version it was built
 * against: rebuild libubx on liburcu updates. Blocks only call ubx_*
 * functions and are not affected. */
#define _LGPL_SOURCE
/* rcu_dereference as a volatile load with address dependency instead
 * of consume, which gcc emits as acquire (a dmb per port access on
 * ARMv7). Requires comparing dereferenced pointers only with NULL or
 * via cmm_ptr_eq. Honored since liburcu 0.15. */
#define URCU_DEREFERENCE_USE_VOLATILE
#include <urcu/urcu-bp.h>

/* the reader lives in liburcu-bp.so: initial-exec avoids a
 * __tls_get_addr/TLS descriptor call per access. When libubx is
 * dlopen'ed, glibc's static TLS surplus has room for it. */
extern __thread struct urcu_bp_reader *urcu_bp_reader
	__attribute__((tls_model("initial-exec")));

#define UBX_RCU_READ_LOCK()	urcu_bp_read_lock()
#define UBX_RCU_READ_UNLOCK()	urcu_bp_read_unlock()
#define UBX_RCU_READ_ONGOING()	urcu_bp_read_ongoing()
#define UBX_RCU_DEREF(p)	rcu_dereference(p)
#define UBX_RCU_ASSIGN(p, v)	rcu_assign_pointer(p, v)
#define UBX_RCU_SYNC()		urcu_bp_synchronize_rcu()
#define UBX_RCU_REGISTER()	urcu_bp_register_thread()

#else

#define UBX_RCU_READ_LOCK()	do {} while (0)
#define UBX_RCU_READ_UNLOCK()	do {} while (0)
#define UBX_RCU_READ_ONGOING()	0
#define UBX_RCU_DEREF(p)	(p)
#define UBX_RCU_ASSIGN(p, v)	((p) = (v))
#define UBX_RCU_SYNC()		do {} while (0)
#define UBX_RCU_REGISTER()	do {} while (0)

#endif /* UBX_URCU */

#endif /* UBX_RCU_H */
