#include <stdint.h>
#include <stddef.h>

#include "u5c_defs.h"

/* normally the user would have to box/unbox his value himself. This
 * would generate a typed, automatic boxing version for
 * conveniance. */
#define typed_write(typename) \
typename typename ## _write(u5c_port* port, typename *x) \
{ \
 u5c_value val; \
 val.data = x; \
 val.type = typename; \
 port->write(val);    \
} \





