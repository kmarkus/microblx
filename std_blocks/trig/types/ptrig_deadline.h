#ifndef _PTRIG_DEADLINE
#define _PTRIG_DEADLINE

#include <stdint.h>

struct ptrig_deadline {
	uint64_t runtime_ns;   /* WCET budget per period; mandatory, must be > 0 */
	uint64_t deadline_ns;  /* relative deadline; 0 = use period_ns */
	uint64_t period_ns;    /* scheduling period; 0 = derive from ptrig period config */
};

#endif /* _PTRIG_DEADLINE */
