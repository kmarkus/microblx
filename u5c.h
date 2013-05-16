#include <stdint.h>
#include <stddef.h>

/* 
 * micro-5c: a 5C compliant distributed function blocks framework in
 * pure C
 */

struct u5c_value;

typedef int(*u5c_serialize)(struct u5c_value*, char* buffer, uint32_t max_size); 
typedef int(*u5c_deserialize)(void*, struct u5c_value*);


/* a typed */
typedef struct u5c_type {
	const char* name;		/* name: dir/header.h/struct foo*/
	uint32_t class;			/* CLASS_STRUCT=1, CLASS_CUSTOM, CLASS_FOO ... */
	u5c_serialize serialize;	/* optional (de-) serialization */
	u5c_deserialize deserialize;
} u5c_type_t;

/* A generic typed value */
typedef struct u5c_value {
	const char* type;	/* a known u5c_type.name */
	u5c_type * type_ptr;	/* link to u5c_type */
	void* data;
	uint32_t size;
} u5c_value_t;


/* anonymous data flow ports in principle hybrid in/out ports, however
 * the respective interaction must support this. (Is this a good idea?)
 */
#define PORT_DIR_IN  1 << 0
#define PORT_DIR_OUT 1 << 1

/* return values for read */
#define PORT_READ_NODATA 1;
#define PORT_READ_NEWDATA 1;

typedef void(*u5c_port_write)(u5c_value* value);
typedef uint32_t(*u5c_port_read)(u5c_value* value);

/* Port */
typedef struct u5c_port {
	const char* name;		/* name of port */
	char* meta;			/* doc, etc. */
	const char* data_type;		/* string data type name */
	const char* data_type_id; 	/* filled in automatically */
	uint32_t attrs;			/* FP_DIR_IN or FP_DIR_OUT */
	
	u5c_port_read read;              
	u5c_port_write **write;         /* a write to a port may
					   result in writing to
					   multiple interactions */
} u5c_port_t;

/* 
 * u5c component
 */
struct u5c_component; /* forward declaration */

/* component methods */
typedef int(*u5c_comp_init) (struct u5c_component*);
typedef int(*u5c_comp_start) (struct u5c_component*);
typedef void(*u5c_comp_exec) (struct u5c_component*);
typedef void(*u5c_comp_stop) (struct u5c_component*);
typedef void(*u5c_comp_cleanup) (struct u5c_component*);

/* better distinguish between compoent type and instance? */
typedef struct u5c_component {
	char* name;		/* name of instance */
	char* type;		/* type name */
	char* meta;		/* doc, etc. */
	u5c_port *ports;
	u5c_value config;

	u5c_comp_init init;         /* initialize device */
	u5c_comp_start start;       /* open device */
	u5c_comp_stop stop;         /* close device */
	u5c_comp_cleanup cleanup;   /* cleanup */

	u5c_comp_exec trigger;

} u5c_component_t;


/* 
 * u5c interaction (aka connection, communication)
 */
typedef struct u5c_interaction {
	char* name;	    /* name of interaction instance */
	char* type;	    /* type of interaction instance */	

	u5c_comp_init init;         /* initialize device */
	u5c_comp_start start;       /* open device */
	u5c_comp_stop stop;         /* close device */
	u5c_comp_cleanup cleanup;   /* cleanup */

	u5c_write write;   /* read and write are implemented by
			      interaction, added to ports upon
			      connection */
	u5c_read read;

	u5c_port *port;     /* the port we are attached to */
	u5c_value *config;  /* configuration of the interaction */
} u5c_interaction_t;

/*
 * u5c trigger component (aka activity)
 */
typedef struct u5c_trigger {
	char* name;
	char* type;

	u5c_comp_init init;         /* initialize device */
	u5c_comp_start start;       /* open device */
	u5c_comp_stop stop;         /* close device */
	u5c_comp_cleanup cleanup;   /* cleanup */

	u5c_component_t ** trig_comps;
} u5c_trigger_t;

/* process global data
 * This contains arrays of known component types, types
 */
typedef struct u5c_node_info {
	char *name;

	struct **u5c_component_t comp_types;	  /* known component types */
	uint32_t comp_types_num;

	struct **u5c_interaction_t inter_types;	  /* known interaction types */
	uint32_t inter_types_num;

	struct **u5c_type_t types;		  /* known types */
	uint32_t types_len;

	struct **u5c_component_t comp_inst;         /* known component instances */
	uint32_t comp_inst_num;
	
} u5c_node_info_t;


#define fblock_init(initfn) \
int __initialize_module(void) { return initfn(); }

#define fblock_cleanup(exitfn) \
void __cleanup_module(void) { exitfn(); }


/* 
 * runtime API 
*/


/* typically used by a main */
int u5c_initialize(u5c_node_info* ni);
void u5c_cleanup(u5c_node_info* ni);

/* used by components, etc */
int u5c_register_component(u5c_node_info *ni, u5c_component* comp);
void u5c_unregister_component(u5c_node_info *ni, u5c_component* comp);

int u5c_register_interaction(u5c_node_info *ni, u5c_interaction* inter);
void u5c_unregister_interaction(u5c_node_info *ni, u5c_interaction* inter)

int u5c_register_interaction(u5c_node_info *ni, u5c_interaction* inter);
void u5c_unregister_interaction(u5c_node_info *ni, u5c_interaction* inter);

int u5c_register_type(u5c_node_info *ni, u5c_type* type);
void u5c_unregister_type(u5c_node_info *ni, u5c_type* type);

int u5c_create_component(char *type);
int u5c_destroy_component(char *name);
int u5c_connect(u5c_component* comp1, u5c_component* comp1, u5c_interaction* ia)
int u5c_disconnect(u5c_component* comp1, u5c_component* comp1, u5c_interaction* ia)


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

