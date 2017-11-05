/*
 * microblx: embedded, real-time safe, reflective function blocks.
 * Copyright (C) 2013,2014 Markus Klotzbuecher <markus.klotzbuecher@mech.kuleuven.be>
 *
 * microblx is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 or (at your option)
 * any later version.
 *
 * microblx is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with eCos; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * As a special exception, if other files instantiate templates or use
 * macros or inline functions from this file, or you compile this file
 * and link it with other works to produce a work based on this file,
 * this file does not by itself cause the resulting work to be covered
 * by the GNU General Public License. However the source code for this
 * file must still be made available in accordance with section (3) of
 * the GNU General Public License.
 *
 * This exception does not invalidate any other reasons why a work
 * based on this file might be covered by the GNU General Public
 * License.
*/

/*
 * ubx type a function definitions.
 *
 * This file is luajit-ffi parsable, the rest goes into ubx.h
 */

/* constants */
enum {
	BLOCK_NAME_MAXLEN	= 100,
	TYPE_HASH_LEN		= 16,   /* md5 */
	TYPE_HASH_LEN_UNIQUE	= 8	/* Number of characters of the
					   type checksum to compare */
};

struct ubx_type;
struct ubx_data;
struct ubx_block;
struct ubx_node_info;

/*
 * module and node
 */
int __ubx_initialize_module(struct ubx_node_info *ni);
void __ubx_cleanup_module(struct ubx_node_info *ni);


/* type and value (data) */

enum {
	TYPE_CLASS_BASIC=1,
	TYPE_CLASS_STRUCT,	/* simple sequential memory struct */
	TYPE_CLASS_CUSTOM	/* requires custom serialization */
};

typedef struct ubx_type
{
	const char* name;		/* name: dir/header.h/struct foo*/
	const char* doc;		/* short documentation string */
	uint32_t type_class;		/* CLASS_STRUCT=1, CLASS_CUSTOM, CLASS_FOO ... */
	unsigned long size;		/* size in bytes */
	void* private_data;		/* private data. */
	uint8_t hash[TYPE_HASH_LEN];
} ubx_type_t;

/* This struct is used to store a reference to the type and contains
   mutable, node specific fields. Since unlike blocks, types are
   mostly immutable, there is no need to take a copy.
 */
typedef struct ubx_type_ref
{
	ubx_type_t *type_ptr;
	unsigned long seqid;		/* remember registration order for ffi parsing */
	UT_hash_handle hh;
} ubx_type_ref_t;

typedef struct ubx_data
{
	const ubx_type_t* type;	/* link to ubx_type */
	unsigned long len;	/* if length> 1 then length of array, else ignored */
	void* data;		/* buffer with size (type->size * length) */
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

/* return values */
enum {
	/* ERROR conditions */
	EPORT_INVALID		= -3,
	EPORT_INVALID_TYPE	= -4,

	/* Registration, etc */
	EINVALID_BLOCK_TYPE	= -5,
	ENOSUCHBLOCK		= -6,
	EALREADY_REGISTERED	= -7,
	EOUTOFMEM		= -8,
	EINVALID_CONFIG		= -9
};

/* Port
 * no distinction between type and value
 */
typedef struct ubx_port
{
	const char* name;		/* name of port */
	const char* doc;		/* documentation string. */

	uint32_t attrs;			/* FP_DIR_IN or FP_DIR_OUT */
	uint32_t state;			/* active/inactive */

	const char* in_type_name;	/* string data type name */
	const char* out_type_name;	/* string data type name */

	ubx_type_t* in_type;		/* resolved in automatically */
	ubx_type_t* out_type;		/* resolved in automatically */

	unsigned long in_data_len;	/* max array size of in/out data */
	unsigned long out_data_len;

	struct ubx_block** in_interaction;
	struct ubx_block** out_interaction;

	/* statistics */
	unsigned long stat_writes;
	unsigned long stat_reades;

	struct ubx_block *block;	/* used for extra typechecks */

	/* todo time stats */
} ubx_port_t;


/*
 * ubx configuration
 */
typedef struct ubx_config
{
	const char* name;
	const char* doc;
	const char* type_name;
	uint32_t attrs;
	ubx_data_t value;
} ubx_config_t;

enum {
	CONFIG_ATTR_RDWR =	0<<0,
	CONFIG_ATTR_RDONLY =	1<<0
};

/*
 * ubx block
 */

/* Block types */
enum {
	BLOCK_TYPE_COMPUTATION=1,
	BLOCK_TYPE_INTERACTION
};

enum {
	BLOCK_STATE_PREINIT,
	BLOCK_STATE_INACTIVE,
	BLOCK_STATE_ACTIVE
};

/* block definition */
typedef struct ubx_block
{
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


/* ubx_module - information to maintain a module */
typedef struct ubx_module
{
	const char* id; /* name or path/name */
	void *handle;
	int(*init)(struct ubx_node_info* ni);
	void(*cleanup)(struct ubx_node_info* ni);
	const char* spdx_license_id;
	UT_hash_handle hh;
} ubx_module_t;


/* node information
 * holds references to all known blocks and types
 */
typedef struct ubx_node_info
{
	const char *name;
	ubx_block_t *blocks; /* instances, only one list */
	ubx_type_ref_t *types; /* known types */
	ubx_module_t *modules;
	unsigned long cur_seqid;
} ubx_node_info_t;


/* OS stuff */
struct ubx_timespec
{
	long int sec;
	long int nsec;
};
