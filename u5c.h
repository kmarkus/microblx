/* 
 * micro-5c: a 5C compliant distributed function blocks framework in
 * pure C
 */

#include <stdint.h>
#include <stddef.h>
#include <uthash.h>

#include "u5c_types.h"


/* anonymous data flow ports in principle hybrid in/out ports, however
 * the respective interaction must support this. (Is this a good idea?)
 */

#define fblock_init(initfn) \
int __initialize_module(void) { return initfn(); }

#define fblock_cleanup(exitfn) \
void __cleanup_module(void) { exitfn(); }


#ifdef DEBUG
# define DBG(fmt, args...) ( fprintf(stderr, "%s: ", __FUNCTION__), \
			     fprintf(stderr, fmt, ##args),	    \
			     fprintf(stderr, "\n") )
#else
# define DBG(fmt, args...)  do {} while (0)
#endif

#define ERR(fmt, args...) ( fprintf(stderr, "%s: ", __FUNCTION__),	\
			    fprintf(stderr, fmt, ##args),		\
			    fprintf(stderr, "\n") )

/* same as ERR but errnum */
#define ERR2(err_num, fmt, args...) ( fprintf(stderr, "%s: ", __FUNCTION__), \
				      fprintf(stderr, fmt, ##args), \
				      fprintf(stderr, ": %s", strerror(err_num)), \
				      fprintf(stderr, "\n") )

