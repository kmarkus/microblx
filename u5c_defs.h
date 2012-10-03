/* 
 * micro-5c: a 5C compliant distributed function blocks framework in pure C
 */

/* A boxed type */
typedef struct {
	char* typename;
	void* data;
} u5c_value;

/* anonymous data flow ports */
static const unsigned int FP_DIR_IN = 0;
static const unsigned int FP_DIR_OUT = 1;

typedef void(*u5c_port_write)(u5c_value* value);
typedef int(*u5c_port_read)(u5c_value* value);

typedef struct {
	const char* name;
	uint32_t dir;		/* FP_DIR_IN or FP_DIR_OUT */
	const char* type_name;
	const char* type_id; 	/* filled in automatically */
	u5c_port_read read;
	u5c_port_write write;
} u5c_port;

/* a serializer provides a mechanism to serialize types to different
   representations */
/* typedef int(*u5c_serialize)(u5c_value*, char* buffer, unsigned int max_len); */
/* typedef	int(*u5c_deserialize)(void*, u5c_value*); */

/* typedef struct { */
/* 	char* name; */
/* 	uint32_t id; */
/* 	u5c_serialize serialize; */
/* 	u5c_deserialize deserialize; */
/* } u5c_serializer; */


/* component */
struct u5c_component; /* frwd decl */

typedef int(*u5c_comp_init) (struct u5c_component*);
typedef int(*u5c_comp_start) (struct u5c_component*);
typedef void(*u5c_comp_stop) (struct u5c_component*);
typedef void(*u5c_comp_cleanup) (struct u5c_component*);

typedef struct u5c_component {
	char* type_name;
	u5c_port *ports;
	u5c_value config;

	u5c_comp_init init;      /* initialize device */
	u5c_comp_start start;      /* open device, ie. */
	u5c_comp_stop stop;
	u5c_comp_cleanup cleanup;
} u5c_component;


