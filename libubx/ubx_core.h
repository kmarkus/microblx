/* ubx core API */

#include "stdint.h"
#include "ubx_types.h"

#ifndef UBX_CORE_H
#define UBX_CORE_H

/* predicates */
int blk_is_proto(const ubx_block_t *b);
int blk_is_instance(const ubx_block_t *b);

int port_is_out(const ubx_port_t *p);
int port_is_in(const ubx_port_t *p);
int port_is_inout(const ubx_port_t *p);
int port_is_cloned(const ubx_port_t *p);
int port_is_dyn(const ubx_port_t *p);

int cfg_is_cloned(const ubx_config_t *c);
int cfg_is_dyn(const ubx_config_t *c);

/* node API */
int ubx_node_init(ubx_node_t *nd, const char *name, uint32_t attrs);
void ubx_node_clear(ubx_node_t *nd);
void ubx_node_cleanup(ubx_node_t *nd);
void ubx_node_rm(ubx_node_t *nd);

int ubx_num_blocks(ubx_node_t *nd);
int ubx_num_types(ubx_node_t *nd);
int ubx_num_modules(ubx_node_t *nd);

/* blocks */
ubx_block_t *ubx_block_get(ubx_node_t *nd, const char *name);
ubx_block_t *ubx_block_create(ubx_node_t *nd, const char *type, const char *name);
void ubx_block_free(ubx_block_t *b);
int ubx_block_rm(ubx_node_t *nd, const char *name);

/* lifecycle hooks */
int ubx_block_init(ubx_block_t *b);
int ubx_block_start(ubx_block_t *b);
int ubx_block_stop(ubx_block_t *b);
int ubx_block_cleanup(ubx_block_t *b);
int ubx_cblock_step(ubx_block_t *b);

/* modules and registration */
int ubx_module_load(ubx_node_t *nd, const char *lib);
ubx_module_t *ubx_module_get(ubx_node_t *nd, const char *lib);
void ubx_module_unload(ubx_node_t *nd, const char *lib);

/**
 * __ubx_initialize_module - module initialization hook
 *
 * This symbol is setup by using the UBX_MODULE_INIT macro.
 *
 * @ni: node handle for the module to register blocks and types
 * @return: 0 if OK, -1 otherwise
 */
int __ubx_initialize_module(struct ubx_node *ni);

/**
 * __ubx_cleanup_module - module cleanuphook
 *
 * This symbol is setup by using the UBX_MODULE_CLEANUP macro.
 *
 * @ni: node handle for the module to unregister blocks and types
 */
void __ubx_cleanup_module(struct ubx_node *ni);

int ubx_block_register(ubx_node_t *nd, struct ubx_proto_block *prot);
int ubx_block_unregister(ubx_node_t *nd, const char *name);

int ubx_type_register(ubx_node_t *nd, ubx_type_t *type);
ubx_type_t *ubx_type_unregister(ubx_node_t *nd, const char *name);

/* types (ubx_type_t) */
ubx_type_t *ubx_type_get(ubx_node_t *nd, const char *name);
ubx_type_t *ubx_type_get_by_hash(ubx_node_t *nd, const uint8_t *hash);
ubx_type_t *ubx_type_get_by_hashstr(ubx_node_t *nd, const char *hashstr);
void ubx_type_hashstr(const ubx_type_t *t, char *buf);

/* data (ubx_data_t) */
ubx_data_t *__ubx_data_alloc(const ubx_type_t *typ, const long array_len);
ubx_data_t *ubx_data_alloc(ubx_node_t *nd, const char *typname, long array_len);
int ubx_data_resize(ubx_data_t *d, long newlen);
void ubx_data_free(ubx_data_t *d);
long data_size(const ubx_data_t *d);
const char *get_typename(const ubx_data_t *data);

/* ports (ubx_port_t) */
int ubx_port_add(ubx_block_t *b, const char *name, const char *doc, const uint32_t attrs,
		 const char *in_type_name, const long in_data_len,
		 const char *out_type_name, const long out_data_len);

int ubx_outport_add(ubx_block_t *b, const char *name, const char *doc, uint32_t attrs,
		    const char *out_type_name, long out_data_len);

int ubx_inport_add(ubx_block_t *b, const char *name, const char *doc, uint32_t attrs,
		   const char *in_type_name, long in_data_len);

ubx_port_t *ubx_port_get(const ubx_block_t *b, const char *name);

int ubx_inport_resize(struct ubx_port *p, long len);
int ubx_outport_resize(struct ubx_port *p, long len);

int ubx_port_rm(ubx_block_t *b, const char *name);
void ubx_port_free(ubx_port_t *p);

long __port_read(const ubx_port_t *port, ubx_data_t *data);
void __port_write(const ubx_port_t *port, const ubx_data_t *data);

/* configs (ubx_config_t) */
ubx_config_t *ubx_config_get(const ubx_block_t *b, const char *name);
ubx_data_t *ubx_config_get_data(const ubx_block_t *b, const char *name);
long ubx_config_get_data_ptr(const ubx_block_t *b, const char *name, void **ptr);
long ubx_config_data_len(const ubx_block_t *b, const char *cfg_name);
int ubx_config_assign(ubx_config_t *c, ubx_data_t *d);
int ubx_config_add2(ubx_block_t *b, const char *name, const char *doc, const char *type_name, uint16_t min, uint16_t max, uint32_t attrs);
int ubx_config_add(ubx_block_t *b, const char *name, const char *doc, const char *type_name);
int ubx_config_rm(ubx_block_t *b, const char *name);

/* connecting ports */
int ubx_port_connect_out(ubx_port_t *p, const ubx_block_t *iblock);
int ubx_port_connect_in(ubx_port_t *p, const ubx_block_t *iblock);
int ubx_port_disconnect_out(ubx_port_t *out_port, const ubx_block_t *iblock);
int ubx_port_disconnect_in(ubx_port_t *in_port, const ubx_block_t *iblock);

int ubx_ports_connect(ubx_port_t *out_port, ubx_port_t *in_port, const ubx_block_t *iblock);
int ubx_ports_disconnect(ubx_port_t *out_port, ubx_port_t *in_port, const ubx_block_t *iblock);

/* rt logging */
void __ubx_log(const int level, const ubx_node_t *nd, const char *src, const char *fmt, ...);

/* misc helpers */
const char *block_state_tostr(unsigned int state);
const char *ubx_version(void);

#endif /* UBX_CORE_H */
