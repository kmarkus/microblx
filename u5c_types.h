/* 
 * u5c defintions
 */

struct u5c_type;
struct u5c_data;
struct u5c_interaction;
struct u5c_component; /* forward declaration */


/* serialization */
typedef struct u5c_serialization {
	const char* name;		/* serialization name */
	const char* type;		/* serialization type */\

	int(*serialize)(struct u5c_data*, char* buffer, uint32_t max_size); 
	int(*deserialize)(void*, struct u5c_data*);
	/* UT_hash_handle hh; */
} u5c_serialization_t;


/* type and value (data) */

enum {
	TYPE_CLASS_STRUCT,	/* simple consecutive struct */
	TYPE_CLASS_CUSTOM	/* requires custom serialization */
};

typedef struct u5c_type {
	const char* name;		/* name: dir/header.h/struct foo*/
	uint32_t class;			/* CLASS_STRUCT=1, CLASS_CUSTOM, CLASS_FOO ... */
	u5c_serialization_t* serializations;
	/* UT_hash_handle hh; */
} u5c_type_t;

typedef struct u5c_data {
	const char* type;		/* a known u5c_type.name */
	u5c_type_t* type_value_ptr;	/* link to u5c_type */
	void* data;
	uint32_t size;
} u5c_data_t;


/* Port attributes */
enum {
	PORT_DIR_IN    =	1 << 0,
	PORT_DIR_OUT   =	1 << 1,
};

enum {
	PORT_ACTIVE = 1 << 0,
};


/* return values for read */
enum {
	PORT_READ_NODATA   = 1,
	PORT_READ_NEWDATA  = 2,

	/* ERROR conditions */
	PORT_INVALID       = -1,
	PORT_INVALID_TYPE  = -2,
};

/* Port
 * no distinction between type and value
 */
typedef struct u5c_port {
	const char* name;		/* name of port */
	const char* in_type_name;	/* string data type name */
	const char* out_type_name;	/* string data type name */

	const u5c_type_t* in_type;    	/* filled in automatically */
	const u5c_type_t* out_type;    	/* filled in automatically */

	char* meta_data;		/* doc, etc. */
	uint32_t attrs;			/* FP_DIR_IN or FP_DIR_OUT */
	uint32_t state;			/* active/inactive */

	/* uint32_t(*read)(struct u5c_port* port, u5c_data_t* value); */
	/* void(**write)(struct u5c_port* port, u5c_data_t* value);          /\* many target interactions, end with NULL *\/ */

	struct u5c_interaction* in_interaction;
	struct u5c_interaction** out_interaction;

	/* statistics */
	uint32_t stat_writes;
	uint32_t stat_reades;
	
	/* todo time stats */
} u5c_port_t;


/*
 * u5c configuration
 */
typedef struct u5c_config {
	const char* name;
	const char* type_name;
	const u5c_data_t *value;
} u5c_config_t;

/* 
 * u5c component
 */

/* component definition */
typedef struct u5c_component_def {
	char* type_name;	/* type name */
	char* meta;		/* doc, etc. */

	u5c_port_t* ports;
	u5c_config_t* configs;

	/* component methods */
	int(*init) (struct u5c_component*);
	int(*start) (struct u5c_component*);
	void(*stop) (struct u5c_component*);
	void(*cleanup) (struct u5c_component*);
	int(*step) (struct u5c_component*);

	struct u5c_component* instances;

} u5c_component_def_t;

/* component instance */
typedef struct u5c_component {
	char *name;
	u5c_component_def_t* component_def;

	u5c_port_t* ports;
	u5c_config_t* configs;

	/* statistics, todo step duration */
	uint32_t stat_num_steps;

	/* UT_hash_handle hh; */
} u5c_component_t;


/* 
 * u5c interaction (aka connection, communication)
 */
typedef struct u5c_interaction_def {
	char* name;	    /* name of interaction instance */

	/* component methods */
	int(*init) (struct u5c_interaction*);
	int(*start) (struct u5c_interaction*);
	void(*stop) (struct u5c_interaction*);
	void(*cleanup) (struct u5c_interaction*);

	/* read and write: these are implemented by interactions and
	 * called by the ports read/write */
	uint32_t(*read)(struct u5c_interaction* interaction, u5c_data_t* value);
	void(*write)(struct u5c_interaction* interaction, u5c_data_t* value);

	u5c_port_t* ports;      /* Interaction outport, e.g. log, overflows, ... */
	u5c_config_t* configs;  /* Interaction configuration */

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

	/* component methods */
	int(*init) (struct u5c_interaction*);
	int(*start) (struct u5c_interaction*);
	void(*stop) (struct u5c_interaction*);
	void(*cleanup) (struct u5c_interaction*);

	/* List of component to trigger */
	u5c_component_t **trig_comps;
} u5c_trigger_t;


/* process global data
 * This contains arrays of known component types, types
 */
typedef struct u5c_node_info {
	char *name;
	/* lua_State *L; */

	u5c_component_def_t **cdefs;	  /* known component types */
	uint32_t cdefs_len;

	u5c_interaction_def_t **idefs;	  /* known interaction types */
	uint32_t idefs_len;

	u5c_type_t **tdefs;		  /* known types */
	uint32_t tdefs_len;

} u5c_node_info_t;


/* 
 * runtime API 
*/


/* initalize a node: typically used by a main */
int u5c_node_init(u5c_node_info_t* ni);
void u5c_node_cleanup(u5c_node_info_t* ni);

/* load libraries */


/* register/unregister different entities */
int u5c_register_cdef(u5c_node_info_t *ni, u5c_component_def_t* comp);
void u5c_unregister_cdef(u5c_node_info_t *ni, u5c_component_def_t* comp);

int u5c_register_type(u5c_node_info_t* ni, u5c_type_t* type);
void u5c_unregister_type(u5c_node_info_t* ni, u5c_type_t* type);

int u5c_register_idef(u5c_node_info_t* ni, u5c_interaction_def_t* inter);
void u5c_unregister_idef(u5c_node_info_t* ni, u5c_interaction_def_t* inter);

/* int u5c_register_trig(u5c_node_info_t* ni, u5c_interaction_t* inter); */
/* void u5c_unregister_trigger(u5c_node_info_t* ni, u5c_interaction_t* inter); */

int u5c_create_component(char *type, char* name);
int u5c_destroy_component(char *name);

int u5c_connect(u5c_component_t* comp1, u5c_component_t* comp2, u5c_interaction_t* ia);
int u5c_disconnect(u5c_component_t* comp1, const char* portname);


/* intra-component  API */
u5c_port_t* u5c_port_get(u5c_component_t* comp, const char* name);
u5c_port_t* u5c_port_add(u5c_component_t* comp);
u5c_port_t* u5c_port_rm(u5c_component_t* comp);

uint32_t __port_read(u5c_port_t* port, u5c_data_t* res);
void __port_write(u5c_port_t* port, u5c_data_t* res);

