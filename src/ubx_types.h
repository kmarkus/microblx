/*
 * ubx type a function definitions.
 *
 * This file is luajit-ffi parsable, the rest goes into ubx.h
 */

enum {
	CONFIG_BLOCK_NAME_MAXLEN = 100,
};

struct ubx_type;
struct ubx_data;
struct ubx_block;
struct ubx_node_info;

/*
 * module and node
 */
int __initialize_module(struct ubx_node_info *ni);
void __cleanup_module(struct ubx_node_info *ni);

/* serialization */
typedef struct ubx_serialization {
	const char* name;		/* serialization name */
	const char* type;		/* serialization type */\

	int(*serialize)(struct ubx_data*, char* buffer, uint32_t max_size);
	int(*deserialize)(void*, struct ubx_data*);
	UT_hash_handle hh;
} ubx_serialization_t;


/* type and value (data) */

enum {
	TYPE_CLASS_BASIC=1,
	TYPE_CLASS_STRUCT,	/* simple sequential memory struct */
	TYPE_CLASS_CUSTOM	/* requires custom serialization */
};

typedef struct ubx_type {
	const char* name;		/* name: dir/header.h/struct foo*/
	unsigned long seqid;		/* remember registration order for ffi parsing */
	uint32_t type_class;		/* CLASS_STRUCT=1, CLASS_CUSTOM, CLASS_FOO ... */
	unsigned long size;		/* size in bytes */
	void* private_data;		/* private data. */
	ubx_serialization_t* serializations;
	UT_hash_handle hh;
} ubx_type_t;


typedef struct ubx_data {
	const ubx_type_t* type;	/* link to ubx_type */
	unsigned long len;	/* if length> 1 then length of array, else ignored */
	void* data;		/* buffer with size (type->size * length) */
} ubx_data_t;


/* Port attributes */
enum {
	PORT_DIR_IN 	= 1 << 0,
	PORT_DIR_OUT 	= 1 << 1,
	PORT_DIR_INOUT  = PORT_DIR_IN & PORT_DIR_OUT,
};

/* Port state */
enum {
	PORT_ACTIVE 	= 1 << 0,
};


/* return values */
enum {
	/* PORT_READ_NODATA   	= -1, */
	/* PORT_READ_NEWDATA  	= -2, */

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
 * no distinction between type and value
 */
typedef struct ubx_port {
	const char* name;		/* name of port */
	const char* meta_data;		/* doc, etc. */

	uint32_t attrs;			/* FP_DIR_IN or FP_DIR_OUT */
	uint32_t state;			/* active/inactive */

	const char* in_type_name;	/* string data type name */
	const char* out_type_name;	/* string data type name */

	ubx_type_t* in_type;		/* resolved in automatically */
	ubx_type_t* out_type;	 	/* resolved in automatically */

	unsigned long in_data_len;	/* max array size of in/out data */
	unsigned long out_data_len;

	struct ubx_block** in_interaction;
	struct ubx_block** out_interaction;

	/* statistics */
	unsigned long stat_writes;
	unsigned long stat_reades;

	/* todo time stats */
} ubx_port_t;


/*
 * ubx configuration
 */
typedef struct ubx_config {
	const char* name;
	const char* meta_data;
	const char* type_name;
	ubx_data_t value;
} ubx_config_t;

/*
 * ubx block
 */

/* Block types */
enum {
	BLOCK_TYPE_COMPUTATION=1,
	BLOCK_TYPE_INTERACTION,
};

enum {
	BLOCK_STATE_PREINIT,
	BLOCK_STATE_INACTIVE,
	BLOCK_STATE_ACTIVE,
};

/* block definition */
typedef struct ubx_block {
	const char* name;	/* type name */
	const char* meta_data;	/* doc, etc. */
	uint32_t type;		/* type, (computation, interaction) */

	ubx_port_t* ports;
	ubx_config_t* configs;

	int block_state;  /* state of lifecycle */
	char *prototype; /* name of prototype, NULL if none */

	struct ubx_node_info* ni;

	int(*init) (struct ubx_block*);
	int(*start) (struct ubx_block*);
	void(*stop) (struct ubx_block*);
	void(*cleanup) (struct ubx_block*);

	/* type dependent block methods */
	union {
		/* COMP_TYPE_COMPUTATION */
		struct {
			void(*step) (struct ubx_block*);

			/* statistics, todo step duration */
			unsigned long stat_num_steps;
		};

		/* COMP_TYPE_INTERACTION */
		struct {
			/* read and write: these are implemented by interactions and
			 * called by the ports read/write */
			int(*read)(struct ubx_block* interaction, ubx_data_t* value);
			void(*write)(struct ubx_block* interaction, ubx_data_t* value);
			unsigned long stat_num_reads;
			unsigned long stat_num_writes;
		};
	};



	void* private_data;

	UT_hash_handle hh;

} ubx_block_t;


/* node information
 * holds references to all known blocks and types
 */
typedef struct ubx_node_info {
	const char *name;
	ubx_block_t *blocks; /* instances, only one list */
	ubx_type_t *types; /* known types */
	unsigned long cur_seqid;
} ubx_node_info_t;
