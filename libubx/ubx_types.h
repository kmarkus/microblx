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

#ifndef UBX_TYPES_H
#define UBX_TYPES_H

/* constants */
enum {
	UBX_NODE_NAME_MAXLEN	= 31,
	UBX_BLOCK_NAME_MAXLEN	= 63,
	UBX_PORT_NAME_MAXLEN	= 31,
	UBX_CONFIG_NAME_MAXLEN  = 31,
	UBX_TYPE_NAME_MAXLEN	= 31,
	UBX_LOG_MSG_MAXLEN	= 127,

	UBX_TYPE_HASH_LEN	= 16,   			/* binary md5 */
	UBX_TYPE_HASHSTR_LEN	= UBX_TYPE_HASH_LEN * 2,	/* hexstring md5 */
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

/**
 * struct ubx_type
 * @ni: ubx_node
 * @sequid: sequential id
 * @type_class: type class of this type
 * @size: size in bytes
 * @hh: UT_hash handle
 * @private_data: private data of this type, if any
 * @doc: short docstring
 * @name: type name
 * @hash: binary hash of this type
 */
typedef struct ubx_type {
	struct ubx_node_info *ni;
	unsigned long seqid;
	uint32_t type_class;
	long size;
	UT_hash_handle hh;
	const void *private_data;
	const char *doc;
	const char name[UBX_TYPE_NAME_MAXLEN + 1];
	uint8_t hash[UBX_TYPE_HASH_LEN + 1];
} ubx_type_t;


/**
 * struct ubx_data
 * @refcnt: reference counting, num_refs = refcnt + 1
 * @type: pointer to struct ubx_type
 * @len: array length of data
 * @data: pointer to data buffer of size (type->size * len)
 */
typedef struct ubx_data {
	int refcnt;
	const ubx_type_t *type;
	long len;
	void *data;
} ubx_data_t;


/* Port attributes */
enum {
	/* Directionality */
	PORT_DIR_IN	= 1 << 0,
	PORT_DIR_OUT	= 1 << 1,
	PORT_DIR_INOUT  = PORT_DIR_IN & PORT_DIR_OUT
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
	EINVALID_CONFIG_TYPE,		/* invalid config type */

	EINVALID_CONFIG_LEN,		/* invalid config array length */
	EINVALID_DATA_LEN,		/* invalid data length */

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

/**
 * struct ubx_port
 *
 * @block: parent block owning this port
 * @attrs: port attributes (PORT_DIR_IN, PORT_DIR_OUT)
 * @in_interaction: input iblocks to read from
 * @out_interaction: output iblocks to write to
 * @in_type: pointer to input type. resolved upon creation.
 * @in_data_len: array length of input data
 * @out_type: pointer to output type. resolved upon creation.
 * @out_data_len: array length of output data
 * @name: name of port
 * @doc: short doc string
 * @in_type_name: ubx type name for input
 * @out_type_name: ubx type name for output
 *
 * The fields @name, @doc, @in_type_name, @out_type_name, @in_data_len
 * and @out_data_len are supported for static port declarations. All
 * others fields are used by port instances and are ignored in
 * declarations.
 *
 * For instances, @in_type_name and @out_type_name are not set. The
 * respective type names can be accessed via port->in_type->name.
 */
typedef struct ubx_port {
	const char name[UBX_PORT_NAME_MAXLEN + 1];
	const char *doc;

	const char *out_type_name;
	const char *in_type_name;

	long in_data_len;
	long out_data_len;

	/* instance fields */
	uint32_t attrs;

	ubx_type_t *in_type;
	ubx_type_t *out_type;

	const struct ubx_block *block;

	struct ubx_block **in_interaction;
	struct ubx_block **out_interaction;
} ubx_port_t;


/**
 * struct ubx_config - represents a block configuration
 * @name: name of config
 * @doc: short docstring
 * @type_name: ubx type name of config
 * @data_len: array length of data (used for declaration only, TODO: get rid of this?)
 * @block: parent block owning this config
 * @type: type of configuration
 * @value: reference to data
 * @attrs: attributes (CHECKLATE, ...)
 * @min: required minimum array length
 * @max: required maximum array length
 *
 * The fields @name @doc @type_name @data_len @min abd @max are
 * supported in static port declarations. All others fields are used
 * at runtime and ignore in declarations.
 *
 * For instances @type_name is unset. The type must be accessed via
 * config.type->name.
 */
typedef struct ubx_config {
	const char name[UBX_CONFIG_NAME_MAXLEN + 1];
	const char *doc;
	const char *type_name;
	long data_len;
	uint16_t min;
	uint16_t max;

	const struct ubx_block *block;

	ubx_type_t *type;
	ubx_data_t *value;

	uint32_t attrs;
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

/**
 * struct ubx_block - microblx function block
 * @name: block name
 * @meta_data: block meta data
 * @type: block type
 * @attrs: bock attributes (BLOCK_ATTR_ACTIVE, ...)
 * @ports: {0} terminated array of static ports
 * @configs: {0} terminated array of static configurations
 * @block_state: current state in block life cycle FSM
 * @prototype: pointer to prototype block (if any)
 * @ni: parent ubx_node
 * @loglevel: ptr to loglevel config (if it exists)
 * @init: init hook
 * @start: start hook
 * @stop: stop hook
 * @cleanup: cleanup hook
 * @step: step hook (only BLOCK_TYPE_COMPUTATION only)
 * @stat_num_steps: step count statistics (only BLOCK_TYPE_COMPUTATION)
 * @read: read hook (only BLOCK_TYPE_INTERACTION)
 * @write: write hook (only BLOCK_TYPE_INTERACTION)
 * @stat_num_reads: read count statistics (only BLOCK_TYPE_INTERACTION)
 * @stat_num_writes: wrte count statistics (only BLOCK_TYPE_INTERACTION)
 * @private_data: pointer to block instance state
 * @hh UT_hash_handle
 *
 * The fields @name, @meta_data, @type, @attrs, @ports @configs and
 * the hooks can be used for static block prototype declarations. All
 * other fields are used at runtime and are ignore for static
 * declarations.
 */
typedef struct ubx_block {
	const char name[UBX_BLOCK_NAME_MAXLEN + 1];
	const char *meta_data;

	uint32_t attrs;
	uint16_t type;
	uint16_t block_state;

	ubx_port_t *ports;
	ubx_config_t *configs;

	const ubx_block_t *prototype;

	struct ubx_node_info *ni;

	const int *loglevel;

	int (*init)(struct ubx_block *b);
	int (*start)(struct ubx_block *b);
	void (*stop)(struct ubx_block *b);
	void (*cleanup)(struct ubx_block *b);

	union {
		/* COMP_TYPE_COMPUTATION */
		struct {
			void (*step)(struct ubx_block *cblock);
			unsigned long stat_num_steps;
		};

		/* COMP_TYPE_INTERACTION */
		struct {
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


/**
 * struct ubx_module - bookkeeping of modules
 * @id: name or path/name
 * @handle: dlopen handle
 * @init: module init function
 * @cleanup: module cleanup function
 * @spdx_license_id: pointer to SDPX license id string
 * @hh: UT_hash_handle
 */
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

/**
 * struct ubx_node_info - node information
 * holds references to all known blocks and types
 * @name: name of node
 * @blocks: UT_hash hash table of all blocks (prototypes and instances)
 * @types: UT_hash hash table of types
 * @modules: UT_hash hash table of loaded modules
 * @cur_seqid: running sequence id for registered types. This is used
 * 	       by the ffi to load types into the ffi in the same order
 * 	       as they were loaded.
 * @attrs: node attributes (ND_MLOCK_ALL, ...)
 * @loglevel: global loglevel
 * @log: pointer to log function
 * @log_data: private state of log function
 */
typedef struct ubx_node_info {
	const char name[UBX_NODE_NAME_MAXLEN + 1];
	ubx_block_t *blocks; /* instances, only one list */
	ubx_type_t *types; /* known types */
	ubx_module_t *modules;
	unsigned long cur_seqid; /* type registration sequence counter */

	uint32_t attrs;
	int loglevel;
	void (*log)(const struct ubx_node_info *inf, const struct ubx_log_msg *msg);
	void *log_data;
} ubx_node_info_t;

/**
 * struct ubx_timespec
 *
 * This is a identical struct as struct timespec used for time
 * handling in microblx. As we can directly register the struct
 * timespec, it would be preferrable to switch all users to that.
 * @sec seconds
 * @nsec nano seconds
 */
struct ubx_timespec {
	long sec;
	long nsec;
};

/**
 * struct ubx_log_msg - ubx log message
 * @level: log level (%UBX_LL_ERR, ...)
 * @ts: timestamp taken at time of logging
 * @src: source of log message (typically block or node name)
 * @msg: log message
 */
struct ubx_log_msg {
	int level;
	struct ubx_timespec ts;
	char src[UBX_BLOCK_NAME_MAXLEN + 1];
	char msg[UBX_LOG_MSG_MAXLEN + 1];
};

#endif /* UBX_TYPES_H */
