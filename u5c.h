#include <stdint.h>
#include <stddef.h>

/* 
 * micro-5c: a 5C compliant distributed function blocks framework in
 * pure C
 */

struct u5c_value;

typedef int(*u5c_serialize)(struct u5c_value*, char* buffer, uint32_t count); 
typedef int(*u5c_deserialize)(void*, struct u5c_value*);


/* a typed */
typedef struct u5c_type {
	const char* name;		/* name */
	uint32_t class;			/* CLASS_STRUCT=1, ... */
	u5c_serialize serialize;	/* optional (de-) serialization */
	u5c_deserialize deserialize;
} u5c_type;

/* A generic typed value */
typedef struct u5c_value {
	const char* type;	/* a known u5c_type.name */
	u5c_type * type_ptr;	/* direct link */
	void* data;
} u5c_value;


/* anonymous data flow ports in principle hybrid in/out ports, however
 * the respective interaction must support this. (Is this a good idea?)
 */
#define PORT_DIR_IN  1 << 0
#define PORT_DIR_OUT 1 << 1

/* return values for read */
#define PORT_READ_NODATA 1;
#define PORT_READ_NEWDATA 1;
#define PORT_READ_OLDDATA 1;

typedef void(*u5c_port_write)(u5c_value* value);
typedef uint32_t(*u5c_port_read)(u5c_value* value);

typedef struct {
	const char* name;		/* name of port */
	char* meta;			/* doc, etc. */
	const char* data_type;		/* string data type name */
	const char* data_type_id; 	/* filled in automatically */
	uint32_t attrs;			/* FP_DIR_IN or FP_DIR_OUT */
	
	u5c_port_read read;
	u5c_port_write write;
} u5c_port;

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
	/* char* type;		/\* type name *\/ */
	char* meta;		/* doc, etc. */
	u5c_port *ports;
	u5c_value config;

	u5c_comp_init init;      /* initialize device */
	u5c_comp_start start;      /* open device, ie. */
	u5c_comp_exec exec;      /* open device, ie. */
	u5c_comp_stop stop;
	u5c_comp_cleanup cleanup;
} u5c_component;


/* 
 * u5c interaction (should this be a component?)
 */
typedef struct {
	char* name;	    /* name of the interaction -> the interaction type */
	u5c_port *port;     /* the port we are attached to */
	u5c_value *config;  /* configuration of the interaction */
} u5c_interaction;

#define fblock_init(initfn) \
int __initialize_module(void) { return initfn(); }

#define fblock_cleanup(exitfn) \
void __cleanup_module(void) { exitfn(); }


/* runtime */

int register_component(u5c_component* comp);
void unregister_component(u5c_component* comp);

