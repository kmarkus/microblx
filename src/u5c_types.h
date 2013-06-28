/*
 * u5c type a function definitions.
 *
 * This file is luajit-ffi parsable, the rest goes into u5c.h
 */

struct u5c_type;
struct u5c_data;
struct u5c_block;
struct u5c_node_info;

/*
 * module and node
 */
int __initialize_module(struct u5c_node_info *ni);
void __cleanup_module(struct u5c_node_info *ni);

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
	TYPE_CLASS_BASIC=1,
	TYPE_CLASS_STRUCT,	/* simple sequential memory struct */
	TYPE_CLASS_CUSTOM	/* requires custom serialization */
};

typedef struct u5c_type {
	const char* name;		/* name: dir/header.h/struct foo*/
	unsigned long seqid;		/* remember registration order for ffi parsing */
	uint32_t type_class;		/* CLASS_STRUCT=1, CLASS_CUSTOM, CLASS_FOO ... */
	unsigned long size;		/* size in bytes */
	void* private_data;		/* private data. */
	u5c_serialization_t* serializations;
	UT_hash_handle hh;
} u5c_type_t;


typedef struct u5c_data {
	const u5c_type_t* type;	/* link to u5c_type */
	unsigned long len;	/* if length> 1 then length of array, else ignored */
	void* data;		/* buffer with size (type->size * length) */
} u5c_data_t;


/* Port attributes */
enum {
	PORT_DIR_IN 	= 1 << 0,
	PORT_DIR_OUT 	= 1 << 1,
};

/* Port state */
enum {
	PORT_ACTIVE 	= 1 << 0,
};


/* return values */
enum {
	PORT_READ_NODATA   	= -1,
	PORT_READ_NEWDATA  	= -2,

	/* ERROR conditions */
	EPORT_INVALID	    	= -3,
	EPORT_INVALID_TYPE  	= -4,

	/* Registration, etc */
	EINVALID_BLOCK_TYPE 	= -5,
	ENOSUCHBLOCK	    	= -6,
	EALREADY_REGISTERED	= -7,
	EOUTOFMEM 		= -8,
	EINVALID_CONFIG		= -9,
};

/* Port
h * no distinction between type and value
 */
typedef struct u5c_port {
	char* name;		/* name of port */
	char* meta_data;		/* doc, etc. */

	uint32_t attrs;			/* FP_DIR_IN or FP_DIR_OUT */
	uint32_t state;			/* active/inactive */

	char* in_type_name;	/* string data type name */
	char* out_type_name;	/* string data type name */

	u5c_type_t* in_type;		/* resolved in automatically */
	u5c_type_t* out_type;	 	/* resolved in automatically */

	unsigned long in_data_len;	/* max array size of in/out data */
	unsigned long out_data_len;

	struct u5c_block** in_interaction;
	struct u5c_block** out_interaction;

	/* statistics */
	unsigned long stat_writes;
	unsigned long stat_reades;

	/* todo time stats */
} u5c_port_t;


/*
 * u5c configuration
 */
typedef struct u5c_config {
	char* name;
	char* type_name;
	u5c_data_t value;
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

enum {
	BLOCK_STATE_PREINIT,
	BLOCK_STATE_INACTIVE,
	BLOCK_STATE_ACTIVE,
};

/* block definition */
typedef struct u5c_block {
	char* name;		/* type name */
	char* meta_data;	/* doc, etc. */
	uint32_t type;		/* type, (computation, interaction, trigger) */

	u5c_port_t* ports;
	u5c_config_t* configs;

	int block_state;  /* state of lifecycle */
	char *prototype; /* name of prototype, NULL if none */


	int(*init) (struct u5c_block*);
	int(*start) (struct u5c_block*);
	void(*stop) (struct u5c_block*);
	void(*cleanup) (struct u5c_block*);

	/* type dependent block methods */
	union {
		/* COMP_TYPE_COMPUTATION */
		struct {
			void(*step) (struct u5c_block*);

			/* statistics, todo step duration */
			unsigned long stat_num_steps;
		};

		/* COMP_TYPE_INTERACTION */
		struct {
			/* read and write: these are implemented by interactions and
			 * called by the ports read/write */
			int(*read)(struct u5c_block* interaction, u5c_data_t* value);
			void(*write)(struct u5c_block* interaction, u5c_data_t* value);
			unsigned long stat_num_reads;
			unsigned long stat_num_writes;
		};

		/* COMP_TYPE_TRIGGER - no special ops */
		struct {
			int(*add)(struct u5c_block* cblock);
			int(*rm)(const char *name);
		};
	};



	void* private_data;

	UT_hash_handle hh;

} u5c_block_t;


/* node information
 * holds references to all known blocks and types
 */
typedef struct u5c_node_info {
	const char *name;
	u5c_block_t *blocks; /* instances, only one list */
	u5c_type_t *types; /* known types */
	unsigned long cur_seqid;
} u5c_node_info_t;
