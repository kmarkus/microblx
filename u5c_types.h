/* 
 * u5c defintions
 */

struct u5c_type;
struct u5c_data;

typedef int(*u5c_serialize)(struct u5c_data*, char* buffer, uint32_t max_size); 
typedef int(*u5c_deserialize)(void*, struct u5c_data*);

/* serialization */
typedef struct u5c_serialization {
	const char* name;		/* serialization name */
	const char* type;		/* serialization type */
	u5c_serialize serialize;	/* optional (de-) serialization */
	u5c_deserialize deserialize;
} u5c_serialization_t;

/* a type defintition */
typedef struct u5c_type {
	const char* name;		/* name: dir/header.h/struct foo*/
	uint32_t class;			/* CLASS_STRUCT=1, CLASS_CUSTOM, CLASS_FOO ... */
	u5c_serialization_t* serializations;
} u5c_type_t;

/* A typed value */
typedef struct u5c_data {
	const char* type;		/* a known u5c_type.name */
	u5c_type_t* type_value_ptr;	/* link to u5c_type */
	void* data;
	uint32_t size;
} u5c_data_t;


/* Port types */
enum {
	PORT_DIR_IN  =	1 << 0,
	PORT_DIR_OUT =	1 << 1
};


typedef void(*u5c_port_write)(u5c_data_t* value);
typedef uint32_t(*u5c_port_read)(u5c_data_t* value);

/* return values for read */
enum {
	PORT_READ_NODATA   = 1,
	PORT_READ_NEWDATA  = 2,

	/* ERROR conditions */
	PORT_INVALID       = -1,
	PORT_INVALID_TYPE  = -2,
};

/* Port definition */
typedef struct u5c_port_def {
	const char* name;		/* name of port */
	const char* in_type_name;	/* string data type name */
	const char* out_type_name;	/* string data type name */

	const u5c_type_t* in_type;    	/* filled in automatically */
	const u5c_type_t* out_type;    	/* filled in automatically */

	uint32_t attrs;			/* FP_DIR_IN or FP_DIR_OUT */
	char* meta_data;		/* doc, etc. */

} u5c_port_def_t;

/* Port instance */
typedef struct u5c_port {
	u5c_port_def_t* port_def;

	int enabled;
	u5c_port_read read;              
	u5c_port_write **write;         /* many target interactions, end with NULL */

	/* statistics */
	uint32_t stat_writes;
	uint32_t stat_reades;
} u5c_port_t;

/*
 * u5c configuration definition
 */
typedef struct u5c_config_def {
	const char* name;
	const char* type_name;
	const u5c_type_t* type;
} u5c_config_def_t;

/* configuration instance */
typedef struct u5c_config {
	const u5c_config_def_t* config_def;
	const u5c_data_t *value;
} u5c_config_t;

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

/* component definition */
typedef struct u5c_component_def {
	char* type_name;	/* type name */
	char* meta;		/* doc, etc. */

	u5c_port_def_t* port_defs;
	u5c_config_def_t* config_defs;

	u5c_comp_init init;         /* initialize device */
	u5c_comp_start start;       /* open device */
	u5c_comp_stop stop;         /* close device */
	u5c_comp_cleanup cleanup;   /* cleanup */

	u5c_comp_exec trigger;

	struct u5c_component* instances;

} u5c_component_def_t;

/* component instance */
typedef struct u5c_component {
	char *name;
	u5c_component_def_t* component_def;

	u5c_port_t* ports;
	u5c_config_t* configs;

} u5c_component_t;

/* 
 * u5c interaction (aka connection, communication)
 */
typedef struct u5c_interaction_def {
	char* name;	    /* name of interaction instance */

	u5c_comp_init init;         /* initialize device */
	u5c_comp_start start;       /* open device */
	u5c_comp_stop stop;         /* close device */
	u5c_comp_cleanup cleanup;   /* cleanup */

	u5c_port_def_t* port_defs;      /* Interaction outport, e.g. log, overflows, ... */
	u5c_config_def_t* config_defs;  /* Interaction configuration */

	u5c_port_write write;   /* read and write are implemented by
			      interaction, added to ports upon
			      connection */
	u5c_port_read read;
} u5c_interaction_def_t;

/* interaction instance */
typedef struct u5c_interaction {
	char* id;
	u5c_interaction_def_t* interaction_def;
	u5c_port_t *ports;     /* the port we are attached to */
	u5c_config_t *configs;  /* configuration of the interaction */
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

	/* lua_State *L; */

	u5c_component_t **comp_types;	  /* known component types */
	uint32_t comp_types_num;

	u5c_interaction_t **inter_types;	  /* known interaction types */
	uint32_t inter_types_num;

	u5c_type_t **types;		  /* known types */
	uint32_t types_len;

	u5c_component_t **comp_inst;         /* known component instances */
	uint32_t comp_inst_num;
	
} u5c_node_info_t;


/* 
 * runtime API 
*/


/* initalize a node: typically used by a main */
int u5c_initialize(u5c_node_info_t* ni);
void u5c_cleanup(u5c_node_info_t* ni);

u5c_port_t* u5c_get_port(u5c_component_t* comp);
u5c_port_t* u5c_add_port(u5c_component_t* comp);
u5c_port_t* u5c_rm_port(u5c_component_t* comp);

/* used by components, etc */
int u5c_register_component(u5c_node_info_t *ni, u5c_component_t* comp);
void u5c_unregister_component(u5c_node_info_t *ni, u5c_component_t* comp);

int u5c_register_interaction(u5c_node_info_t* ni, u5c_interaction_t* inter);
void u5c_unregister_interaction(u5c_node_info_t* ni, u5c_interaction_t* inter);

int u5c_register_interaction(u5c_node_info_t* ni, u5c_interaction_t* inter);
void u5c_unregister_interaction(u5c_node_info_t* ni, u5c_interaction_t* inter);

int u5c_register_type(u5c_node_info_t* ni, u5c_type_t* type);
void u5c_unregister_type(u5c_node_info_t* ni, u5c_type_t* type);

int u5c_create_component(char *type, char* name);
int u5c_destroy_component(char *name);
int u5c_connect(u5c_component_t* comp1, u5c_component_t* comp2, u5c_interaction_t* ia);
int u5c_disconnect(u5c_component_t* comp1, u5c_component_t* comp2, u5c_interaction_t* ia);

