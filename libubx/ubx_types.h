/*
 * microblx: embedded, realtime safe, reflective function blocks.
 *
 * Copyright (C) 2013,2014 Markus Klotzbuecher
 *			   <markus.klotzbuecher@mech.kuleuven.be>
 * Copyright (C) 2014-2020 Markus Klotzbuecher <mk@mkio.de>
 *
 * SPDX-License-Identifier: MPL-2.0
 *
 * ubx type a function definitions.
 *
 * This file is luajit-ffi parsable, the rest goes into ubx.h
 */

/* constants */
enum {
	BLOCK_NAME_MAXLEN	= 30,
	TYPE_NAME_MAXLEN	= 30,
	TYPE_HASH_LEN		= 16,   		/* binary md5 */
	TYPE_HASHSTR_LEN	= TYPE_HASH_LEN*2,   	/* hexstring md5 */
	LOG_MSG_MAXLEN		= 120,
};

struct ubx_type;
struct ubx_data;
struct ubx_block;
struct ubx_node_info;
struct ubx_log_msg;

/*
 * module and node
 */
int __ubx_initialize_module(struct ubx_node_info *ni);
void __ubx_cleanup_module(struct ubx_node_info *ni);


/* type and value (data) */

enum {
	TYPE_CLASS_BASIC = 1,
	TYPE_CLASS_STRUCT,	/* simple sequential memory struct */
	TYPE_CLASS_CUSTOM	/* requires custom serialization */
};

typedef struct ubx_type {
	const char *name;		/* name: dir/header.h/struct foo*/
	const char *doc;		/* short documentation string */
	struct ubx_node_info *ni;	/* node */
	unsigned long seqid;
	uint32_t type_class;		/*
					 * CLASS_STRUCT=1, CLASS_CUSTOM,
					 * CLASS_FOO ...
					 */

	long size;			/* size in bytes */
	const void *private_data;	/* private data. */
	uint8_t hash[TYPE_HASH_LEN+1];
	UT_hash_handle hh;
} ubx_type_t;

typedef struct ubx_data {
	int refcnt;		/* reference counting, num refs = refcnt + 1 */
	const ubx_type_t *type;	/* link to ubx_type */
	long len;		/*
				 * if length> 1 then length of array, else
				 * ignored
				 */
	void *data;		/* buffer with size (type->size * length) */
} ubx_data_t;


/* Port attributes */
enum {
	/* Directionality */
	PORT_DIR_IN	= 1 << 0,
	PORT_DIR_OUT	= 1 << 1,
	PORT_DIR_INOUT  = PORT_DIR_IN & PORT_DIR_OUT
};

/* Port state */
enum {
	PORT_ACTIVE	= 1 << 0
};

enum {
	PORT_READ_NODATA		= 0,
	PORT_READ_BUFF_TOO_SMALL	= -1,
	PORT_READ_DROPPED		= -2
};

/* return error codes */
enum {
	EINVALID_BLOCK = -32767,	/* invalid block */
	EINVALID_PORT,			/* invalid port */
	EINVALID_CONFIG,		/* invalid config */
	EINVALID_TYPE,			/* invalid config */

	EINVALID_BLOCK_TYPE,		/* invalid block type */
	EINVALID_PORT_TYPE,		/* invalid port type */
	EINVALID_CONFIG_TYPE,		/* invalid config type */

	EINVALID_CONFIG_LEN,		/* invalid config array length */

	EINVALID_PORT_DIR,		/* invalid port direction */

	EINVALID_ARG,			/* UBX EINVAL */
	EWRONG_STATE,			/* invalid FSM state */
	ENOSUCHENT,			/* no such entity */
	EENTEXISTS,			/* entity exists already */
	EALREADY_REGISTERED,		/* entity already registered */
	ETYPE_MISMATCH,			/* mismatching types */
	EOUTOFMEM,			/* UBX ENOMEM */

};

enum {
	UBX_LOGLEVEL_EMERG	= 0,	/* system unusable */
	UBX_LOGLEVEL_ALERT	= 1,	/* immediate action required */
	UBX_LOGLEVEL_CRIT	= 2,	/* critical */
	UBX_LOGLEVEL_ERR	= 3,	/* error */
	UBX_LOGLEVEL_WARN	= 4,	/* warning conditions */
	UBX_LOGLEVEL_NOTICE	= 5,	/* normal but significant */
	UBX_LOGLEVEL_INFO	= 6,	/* info msg */
	UBX_LOGLEVEL_DEBUG	= 7	/* debug messages */
};

/* Port
 * no distinction between type and value
 */
typedef struct ubx_port {
	const char *name;		/* name of port */
	const char *doc;		/* documentation string. */

	uint32_t attrs;			/* FP_DIR_IN or FP_DIR_OUT */
	uint32_t state;			/* active/inactive */

	const char *in_type_name;	/* string data type name */
	const char *out_type_name;	/* string data type name */

	ubx_type_t *in_type;		/* resolved in automatically */
	ubx_type_t *out_type;		/* resolved in automatically */

	long in_data_len;		/* max array size of in/out data */
	long out_data_len;

	struct ubx_block **in_interaction;
	struct ubx_block **out_interaction;

	/* statistics */
	unsigned long stat_writes;
	unsigned long stat_reads;

	const struct ubx_block *block;	/* block owning this port */

	/* todo time stats */
} ubx_port_t;


/*
 * ubx configuration
 */
typedef struct ubx_config {
	const char *name;
	const char *doc;
	const char *type_name;
	ubx_type_t *type;
	const struct ubx_block *block;	/* parent block */

	ubx_data_t *value;		/* reference to actual value */
	long data_len;			/* array size of value */

	uint32_t attrs;

	/* */
	uint16_t min;
	uint16_t max;
} ubx_config_t;

enum {
	CONFIG_ATTR_RDONLY =	 1<<0,
	CONFIG_ATTR_CHECKLATE =  2<<1	/*
					 * perform checking of min,
					 * max before start instead of
					 * before init hook
					 */
};

/*
 * ubx block
 */

/* Block types */
enum {
	BLOCK_TYPE_COMPUTATION = 1,
	BLOCK_TYPE_INTERACTION
};

/* Block attributes */
enum {
	BLOCK_ATTR_TRIGGER =	1<<0,	/* block is a trigger */
	BLOCK_ATTR_ACTIVE =	1<<1,	/* block is active (has a thread) */
};

/* block states */
enum {
	BLOCK_STATE_PREINIT,
	BLOCK_STATE_INACTIVE,
	BLOCK_STATE_ACTIVE
};

/* block definition */
typedef struct ubx_block {
	const char *name;	/* type name */
	const char *meta_data;	/* doc, etc. */
	uint32_t type;		/* type, (computation, interaction) */
	uint32_t attrs;		/* attributes */

	ubx_port_t *ports;
	ubx_config_t *configs;

	int block_state;  /* state of lifecycle */
	char *prototype; /* name of prototype, NULL if none */

	struct ubx_node_info *ni;

	const int *loglevel;

	int (*init)(struct ubx_block *b);
	int (*start)(struct ubx_block *b);
	void (*stop)(struct ubx_block *b);
	void (*cleanup)(struct ubx_block *b);

	/* type dependent block methods */
	union {
		/* COMP_TYPE_COMPUTATION */
		struct {
			void (*step)(struct ubx_block *cblock);

			/* statistics, todo step duration */
			unsigned long stat_num_steps;
		};

		/* COMP_TYPE_INTERACTION */
		struct {
			/*
			 * read and write: these are implemented by
			 * interactions and called by the port's read/write
			 */
			long (*read)(struct ubx_block *iblock,
				     ubx_data_t *value);
			void (*write)(struct ubx_block *iblock,
				      const ubx_data_t *value);
			unsigned long stat_num_reads;
			unsigned long stat_num_writes;
		};
	};



	void *private_data;

	UT_hash_handle hh;

} ubx_block_t;


/* ubx_module - information to maintain a module */
typedef struct ubx_module {
	const char *id; /* name or path/name */
	void *handle;
	int (*init)(struct ubx_node_info *ni);
	void (*cleanup)(struct ubx_node_info *ni);
	const char *spdx_license_id;
	UT_hash_handle hh;
} ubx_module_t;


/* node flags */
enum {
	ND_MLOCK_ALL = 1 << 0,
	ND_DUMPABLE =  1 << 1,
};

/* node information
 * holds references to all known blocks and types
 */
typedef struct ubx_node_info {
	const char *name;
	ubx_block_t *blocks; /* instances, only one list */
	ubx_type_t *types; /* known types */
	ubx_module_t *modules;
	unsigned long cur_seqid; /* type registration sequence counter */

	uint32_t attrs;
	int loglevel;
	void (*log)(const struct ubx_node_info *inf, const struct ubx_log_msg *msg);
	void *log_data;
} ubx_node_info_t;

/* OS stuff */
struct ubx_timespec {
	long sec;
	long nsec;
};

/**
 * struct ubx_log_msg - ubx log message
 * @level:	log level (%UBX_LL_ERR, ...)
 * @ts:		timestamp taken at time of logging
 * @src:	source of log message (typically block or node name)
 * @msg:	log message
 */
struct ubx_log_msg {
	int level;
	struct ubx_timespec ts;
	char src[BLOCK_NAME_MAXLEN + 1];
	char msg[LOG_MSG_MAXLEN + 1];
};
