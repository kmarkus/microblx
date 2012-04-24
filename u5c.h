/* 
 * micro-5c: a 5C compliant framework in pure C
 */

#include <stdint.h>
#include <stddef.h>

typedef uint32_t u5c_typeid;

/* a typed value */
typedef struct {
	u5c_typeid type_id;
	void* data;
} u5c_value;

/* is this to specific to dataflow? 
 * should we generalize this to a communication mechanism?
 */

#define CONFIG_ATTR_RONLY 1
#define CONFIG_ATTR_WONLY 2
#define CONFIG_ATTR_RW 3

typedef struct {
	char* name;
	char* typename;
	uint32_t attrs;
	u5c_typeid typeid;
	/* where to store value? */
} u5c_config;

/* anonymous data flow ports */
#define FP_DIR_IN	0
#define FP_DIR_OUT	1

typedef void(*u5c_df_send)(u5c_value* value);
typedef uint32_t(*u5c_df_recv)(u5c_value* value);

typedef struct {
	char* name;
	uint32_t dir;	/* FP_DIR_IN or FP_DIR_OUT */
	char* typename;
	u5c_typeid typeid; 	/* filled in automatically */
	u5c_df_send send;
	u5c_df_recv recv;
} u5c_flowport;

/* synchronous request reply
 * can be routed via network or
 */
typedef void(*u5c_op_call)(u5c_value* in_value);

typedef struct {
	char *name;
} u5c_operation;

/* the interface to a communication protocol */
typedef struct {
	char* name;
	uint32_t comm_id;
} u5c_communication;

/* a serializer provides a mechanism to serialize types to different
   representations */
typedef int(*u5c_serialize)(u5c_value*, char* buffer, unsigned int max_len);
typedef	int(*u5c_deserialize)(void*, u5c_value*);

typedef struct {
	char* name;
	uint32_t id;
	u5c_serialize serialize;
	u5c_deserialize deserialize;
} u5c_serializer;


/* component */
struct u5c_component; /* frwd decl */

typedef int(*u5c_comp_init) (struct u5c_component*);
typedef int(*u5c_comp_open) (struct u5c_component*);
typedef int(*u5c_comp_start) (struct u5c_component*);
typedef void(*u5c_comp_stop) (struct u5c_component*);
typedef void(*u5c_comp_close) (struct u5c_component*);
typedef void(*u5c_comp_cleanup) (struct u5c_component*);

typedef struct u5c_component {
	char* type;
	u5c_flowport *flowports;
	u5c_operation* operations;
	u5c_config* configs;

	u5c_comp_init init;
	u5c_comp_open open;
	u5c_comp_start start;
	u5c_comp_stop stop;
	u5c_comp_close close;
	u5c_comp_cleanup cleanup;
} u5c_component;

