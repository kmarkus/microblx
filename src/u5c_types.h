/*
 * u5c defintions
 */

struct u5c_type;
struct u5c_data;
struct u5c_block;

/* serialization */
typedef struct u5c_serialization {
	const char* name;		/* serialization name */
	const char* type;		/* serialization type */\

	int(*serialize)(struct u5c_data*, char* buffer, uint32_t max_size);
	int(*deserialize)(void*, struct u5c_data*);
	UT_hash_handle hh;
} u5c_serialization_t;


/* type and value (data) */

enum {
	TYPE_CLASS_STRUCT,	/* simple consecutive struct */
	TYPE_CLASS_CUSTOM	/* requires custom serialization */
};

typedef struct u5c_type {
	const char* name;		/* name: dir/header.h/struct foo*/
	uint32_t class;			/* CLASS_STRUCT=1, CLASS_CUSTOM, CLASS_FOO ... */
	unsigned long size;		/* size in bytes */
	u5c_serialization_t* serializations;
	UT_hash_handle hh;
} u5c_type_t;


typedef struct u5c_data {
	const u5c_type_t* type;	/* link to u5c_type */
	unsigned long length;	/* if length> 1 then length of array, else ignored */
	void* data;		/* buffer with size (type->size * length) */
} u5c_data_t;


/* Port attributes */
enum {
	PORT_DIR_IN    =	1 << 0,
	PORT_DIR_OUT   =	1 << 1,
};

/* Port state */
enum {
	PORT_ACTIVE = 1 << 0,
};


/* return values */
enum {
	PORT_READ_NODATA   = 1,
	PORT_READ_NEWDATA  = 2,

	/* ERROR conditions */
	EPORT_INVALID       = -1,
	EPORT_INVALID_TYPE  = -2,

	/* Registration, etc */
	EINVALID_BLOCK_TYPE = -3,
	ENOSUCHBLOCK        = -4,
	EALREADY_REGISTERED = -5,
};

/* Port
 * no distinction between type and value
 */
typedef struct u5c_port {
	char* name;		/* name of port */
	char* in_type_name;	/* string data type name */
	char* out_type_name;	/* string data type name */

	u5c_type_t* in_type;		/* filled in automatically */
	u5c_type_t* out_type;	 	/* filled in automatically */

	char* meta_data;		/* doc, etc. */
	uint32_t attrs;			/* FP_DIR_IN or FP_DIR_OUT */
	uint32_t state;			/* active/inactive */

	struct u5c_block* in_interaction;
	struct u5c_block* out_interaction;

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
	void *data;
} u5c_config_t;

/*
 * u5c block
 */

/* Block types */
enum {
	BLOCK_TYPE_COMPUTATION=1,
	BLOCK_TYPE_INTERACTION,
	BLOCK_TYPE_TRIGGER,
};

/* block definition */
typedef struct u5c_block {
	char* name;		/* type name */
	char* meta_data;	/* doc, etc. */
	uint32_t type;		/* type, (computation, interaction, trigger) */

	u5c_port_t* ports;
	const u5c_config_t* configs;


	uint32_t state;  /* state of lifecycle */
	char *prototype; /* name of prototype, NULL if none */


	int(*init) (struct u5c_block*);
	int(*start) (struct u5c_block*);
	void(*stop) (struct u5c_block*);
	void(*cleanup) (struct u5c_block*);

	/* type dependent block methods */
	union {
		/* COMP_TYPE_COMPUTATION */
		void(*step) (struct u5c_block*);

		/* COMP_TYPE_INTERACTION */
		struct {
			/* read and write: these are implemented by interactions and
			 * called by the ports read/write */
			int(*read)(struct u5c_block* interaction, u5c_data_t* value);
			void(*write)(struct u5c_block* interaction, u5c_data_t* value);
		};

		/* COMP_TYPE_TRIGGER - no special ops */
		struct {
			int(*add)(struct u5c_block* cblock);
			int(*rm)(const char *name);
		};
	};


	/* statistics, todo step duration */
	uint32_t stat_num_steps;

	void* private_data;

	UT_hash_handle hh;

} u5c_block_t;


/* node information
 * holds references to all known blocks and types
 */
typedef struct u5c_node_info {
	const char *name;

	u5c_block_t *cblocks;	/* known computation blocks */
	u5c_block_t *iblocks;	/* known interaction blocks */
	u5c_block_t *tblocks;	/* known trigger blocks */
	u5c_type_t *types;	/* known types */
} u5c_node_info_t;


/*
 * runtime API
 */

const char* get_typename(u5c_data_t* data);

/* initalize a node: typically used by a main */
int u5c_node_init(u5c_node_info_t* ni);
void u5c_node_cleanup(u5c_node_info_t* ni);

int u5c_num_cblocks(u5c_node_info_t* ni);
int u5c_num_iblocks(u5c_node_info_t* ni);
int u5c_num_tblocks(u5c_node_info_t* ni);
int u5c_num_types(u5c_node_info_t* ni);

/* register/unregister different entities */
int u5c_block_register(u5c_node_info_t *ni, u5c_block_t* block);
u5c_block_t* u5c_block_unregister(u5c_node_info_t* ni, uint32_t type, const char* name);

/* create and destroy different blocks */
u5c_block_t* u5c_block_create(u5c_node_info_t *ni, uint32_t block_type, const char *name, const char *type);
int u5c_block_destroy(u5c_node_info_t *ni, uint32_t block_type, const char *name);

int u5c_type_register(u5c_node_info_t* ni, u5c_type_t* type);
int u5c_type_unregister(u5c_node_info_t* ni, u5c_type_t* type);

u5c_block_t* u5c_cblock_create(u5c_node_info_t* ni, const char *type, const char* name);

/* connect ports */
int u5c_connect(u5c_port_t* p1, u5c_port_t* p2, u5c_block_t* iblock);
/* int u5c_disconnect(u5c_block_t* comp1, const char* portname); */
/* int u5c_disconnect_link(u5c_block_t* cblock1, const char* port1, u5c_block_t* cblock2, const char *port2); */

/* intra-block API */
u5c_port_t* u5c_port_get(u5c_block_t* comp, const char *name);
u5c_port_t* u5c_port_add(u5c_block_t* comp, const char *name, const char *type);
u5c_port_t* u5c_port_rm(u5c_block_t* comp);
/* FOR_EACH_INPORT, FOR_EACH_OUTPORT */

uint32_t __port_read(u5c_port_t* port, u5c_data_t* res);
void __port_write(u5c_port_t* port, u5c_data_t* res);

/* module init/cleanup */
int __initialize_module(u5c_node_info_t* ni);
void __cleanup_module(u5c_node_info_t* ni);
