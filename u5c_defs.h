/* 
 * micro-5c: a 5C compliant distributed function blocks framework in
 * pure C
 */

/* A generic typed value */
typedef struct {
	uint32_t type_id;
	void* data;
} u5c_value;


/* anonymous data flow ports */
#define PORT_DIR_IN 0
#define PORT_DIR_OUT 1

/* return values for read */
#define PORT_READ_NODATA 1;
#define PORT_READ_NEWDATA 1;
#define PORT_READ_OLDDATA 1;


typedef void(*u5c_port_write)(u5c_value* value);
typedef uint32_t(*u5c_port_read)(u5c_value* value);

typedef struct {
	const char* name;		/* name of port */
	const char* data_type;		/* string data type name */
	const char* data_type_id; 	/* filled in automatically */
	uint32_t direction;		/* FP_DIR_IN or FP_DIR_OUT */
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

typedef struct u5c_component {
	char* name;
	char* doc;
	u5c_port *ports;
	u5c_value *config;

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

/*
 * u5c: serializer: a serializer provides a mechanism to
 * serialize/unserialize any type.
 */
/*
typedef int(*u5c_serialize)(u5c_value*, char* buffer, unsigned int max_len); 
typedef int(*u5c_deserialize)(void*, u5c_value*);

typedef struct {
	char* name;
	uint32_t id;
	u5c_serialize serialize;
	u5c_deserialize deserialize;
} u5c_serializer;
*/

