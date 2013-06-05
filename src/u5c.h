/* 
 * micro-5c: a 5C compliant distributed function blocks framework in
 * pure C
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>

#include <uthash.h>

#include "u5c_types.h"


/* module init, cleanup */
#define module_init(initfn) \
int __initialize_module(u5c_node_info_t* ni) { return initfn(ni); }

#define module_cleanup(exitfn) \
void __cleanup_module(u5c_node_info_t* ni) { exitfn(ni); }


/* normally the user would have to box/unbox his value himself. This
 * would generate a typed, automatic boxing version for
 * convenience. */
#define gen_write(function_name, typename) \
void function_name(u5c_port_t* port, typename *outval) \
{ \
 u5c_data_t val; \
 if(port==NULL) { ERR("port is NULL"); return; } \
 val.data = outval; \
 val.type = port->out_type; \
 __port_write(port, &val);	\
} \

/* generate a typed read function: arguments to the function are the
 * port and a pointer to the result value. 
 */
#define gen_read(function_name, typename) \
uint32_t function_name(u5c_port_t* port, typename *inval) \
{ \
 u5c_data_t val; 		\
 val.data = inval;	  	\
 return __port_read(port, &val);	\
} \


#define data_len(d) (d->length * d->type->size)

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

