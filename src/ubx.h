/* 
 * micro-5c: a 5C compliant distributed function blocks framework in
 * pure C
 */

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <assert.h>
#include <uthash.h>

#include "ubx_types.h"
#include "ubx_proto.h"


/* module init, cleanup */
#define module_init(initfn) \
int __initialize_module(ubx_node_info_t* ni) { return initfn(ni); }

#define module_cleanup(exitfn) \
void __cleanup_module(ubx_node_info_t* ni) { exitfn(ni); }

/* type definition helpers */
#define def_basic_ctype(typename) { .name=#typename, .type_class=TYPE_CLASS_BASIC, .size=sizeof(typename) }

#define def_struct_type(module, typename, hexdata) \
{ 					\
	.name=module "/" #typename, 	\
	.type_class=TYPE_CLASS_STRUCT,	\
	.size=sizeof(typename),		\
	.private_data=hexdata, 		\
}


/* normally the user would have to box/unbox his value himself. This
 * would generate a typed, automatic boxing version for
 * convenience. */
#define def_write_fun(function_name, typename) 		\
void function_name(ubx_port_t* port, typename *outval) 	\
{ 							\
 ubx_data_t val; 					\
 if(port==NULL) { ERR("port is NULL"); return; } 	\
 assert(strcmp(#typename, port->out_type_name)==0); 	\
 val.data = outval; 					\
 val.type = port->out_type; 				\
 val.len=1;						\
 __port_write(port, &val);				\
} 							\

/* generate a typed read function: arguments to the function are the
 * port and a pointer to the result value. 
 */
#define def_read_fun(function_name, typename) 		\
int32_t function_name(ubx_port_t* port, typename *inval) \
{ 							\
 ubx_data_t val; 					\
 if(port==NULL) { ERR("port is NULL"); return -1; } 	\
 assert(strcmp(#typename, port->in_type_name)==0);	\
 val.type=port->in_type;				\
 val.data = inval;	  				\
 return __port_read(port, &val);			\
} 							\

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


#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#ifdef __cplusplus
}
#endif
