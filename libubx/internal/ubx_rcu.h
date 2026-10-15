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

/* no _LGPL_SOURCE: libubx is MPL-2.0, so read lock/unlock stay
 * library calls. INLINE_SMALL_FUNCTIONS is allowed for any license
 * and inlines rcu_dereference/rcu_assign_pointer. */
#define URCU_INLINE_SMALL_FUNCTIONS
#include <urcu/urcu-bp.h>

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
