
/*
 * Macros for safe interaction with C types
 */

/* type definition helpers */
#define def_basic_ctype(typename)		\
{						\
	.name = #typename,			\
	.type_class = TYPE_CLASS_BASIC,		\
	.size = sizeof(typename)		\
}

#define def_struct_type(typename, hexdata)	\
{						\
	.name = QUOTE(typename),		\
	.type_class = TYPE_CLASS_STRUCT,	\
	.size = sizeof(typename),		\
	.private_data = (void *)hexdata,	\
}

/*
 * Define port read functions
 */
#define def_port_readers(FUNCNAME, TYPENAME)			   \
long FUNCNAME ## _array(const ubx_port_t* p, TYPENAME* val, const int len) \
{								   \
	ubx_data_t data;					   \
	static ubx_type_t *type = NULL;				   \
								   \
	if (p == NULL || p->block == NULL) {			   \
		ERR("invalid input port");			   \
		return EINVALID_PORT;				   \
	} else if (p->in_type == NULL) {				\
		ubx_err(p->block, "%s: port %s not an input port", __func__, p->name); \
		return EINVALID_PORT_DIR;				\
	}								\
									\
	/* lookup port in_type on first call or when different
	 * (could be due to type module reload */			\
	if (p->in_type != type) {					\
		type = ubx_type_get(p->block->ni, QUOTE(TYPENAME));	\
									\
		if (type == NULL) {					\
			ubx_err(p->block, "%s: unregistered type " QUOTE(TYPENAME), __func__); \
			return EINVALID_TYPE;				\
		}							\
									\
		/* check type */					\
		if (p->in_type != type) {				\
			ubx_err(p->block, "%s: ETYPE_MISMATCH: expected %s but port %s is %s", \
				__func__, QUOTE(TYPENAME), p->name, p->in_type_name); \
			return ETYPE_MISMATCH;				\
		}							\
	}								\
									\
	data.data = (void*) val;					\
	data.type = type;						\
	data.len = len;							\
	return __port_read(p, &data);					\
}									\
									\
long FUNCNAME(const ubx_port_t* p, TYPENAME* val)			\
{									\
	return FUNCNAME ## _array(p, val, 1);				\
}									\

/*
 * Define port write functions
 */
#define def_port_writers(FUNCNAME, TYPENAME)				\
int FUNCNAME ## _array(const ubx_port_t *p, const TYPENAME *val, const int len) \
{									\
	ubx_data_t data;						\
	static ubx_type_t *type = NULL;					\
									\
	if (p == NULL || p->block == NULL) {				\
		ERR("invalid output port");				\
		return EINVALID_PORT;					\
	} else if (p->out_type == NULL) {				\
		ubx_err(p->block, "%s: port %s not an output port", __func__, p->name); \
		return EINVALID_PORT_DIR;				\
	}								\
									\
	/* lookup port in_type on first call or when different
	 * (could be due to type module reload */			\
	if (p->out_type != type) {					\
		type = ubx_type_get(p->block->ni, QUOTE(TYPENAME));	\
									\
		if (type == NULL) {					\
			ubx_err(p->block, "%s: unregistered type " QUOTE(TYPENAME), __func__); \
			return EINVALID_TYPE;				\
		}							\
									\
		/* check type */					\
		if (p->out_type != type) {				\
			ubx_err(p->block, "%s: ETYPE_MISMATCH: expected %s but port %s is %s", \
				__func__, QUOTE(TYPENAME), p->name, p->out_type_name); \
			return ETYPE_MISMATCH;				\
		}							\
	}								\
									\
	data.data = (void*) val;					\
	data.type = type;						\
	data.len = len;							\
	__port_write(p, &data);						\
	return 0;							\
}									\
									\
int FUNCNAME(const ubx_port_t *p, const TYPENAME *val)			\
{									\
	return FUNCNAME ## _array(p, val, 1);				\
}									\


#define def_cfg_getptr_fun(FUNCNAME, TYPENAME)				\
long FUNCNAME(const ubx_block_t *b,					\
	      const char *cfg_name,					\
	      const TYPENAME **valptr)					\
{									\
	const ubx_config_t *c;						\
	static ubx_type_t *type = NULL;					\
									\
	c = ubx_config_get(b, cfg_name);				\
									\
	if (c == NULL) {						\
		ubx_err(b, "EINVALID_CONFIG: %s", cfg_name);		\
		return EINVALID_CONFIG;					\
	}								\
									\
	/* lookup config type on first call or when different
	 * (could be due to type module reload */			\
	if (c->type != type) {						\
		type = ubx_type_get(b->ni, QUOTE(TYPENAME));		\
									\
		if (type == NULL) {					\
			ubx_err(b, "%s: unregistered type " QUOTE(TYPENAME), __func__); \
			return EINVALID_TYPE;				\
		}							\
									\
		if (c->type != type) {					\
			ubx_err(b, "%s: ETYPE_MISMATCH: expected %s but config %s is %s", \
				__func__, QUOTE(TYPENAME), cfg_name, c->type->name); \
			return ETYPE_MISMATCH;				\
		}							\
	}								\
									\
	return ubx_config_get_data_ptr(b, cfg_name, (void **) valptr);	\
}


/* generate both port readers and writers */
#define def_port_accessors(SUFFIX, TYPENAME) \
def_port_writers(write_ ## SUFFIX, TYPENAME) \
def_port_readers(read_ ## SUFFIX, TYPENAME)

/* generate port and config accessors */
#define def_type_accessors(SUFFIX, TYPENAME) \
def_port_accessors(SUFFIX, TYPENAME) \
def_cfg_getptr_fun(cfg_getptr_ ## SUFFIX, TYPENAME)

/* C++ helpers */
#ifdef __cplusplus
#include <type_traits>

/* generate overloaded port RW and config accessors */
#define gen_class_accessors(SUFFIX, CLASS, UBXTYPE)			\
                                                                        \
long portRead(const ubx_port_t *p, CLASS* x, const int len)		\
{                                                                       \
	return read_ ## SUFFIX ## _array(p, (UBXTYPE*) x, len);		\
}                                                                       \
									\
long portRead(const ubx_port_t *p, CLASS* x)				\
{                                                                       \
	return read_ ## SUFFIX(p, (UBXTYPE*) x);			\
}                                                                       \
									\
long portWrite(const ubx_port_t *p, const CLASS* x, const int len)	\
{                                                                       \
	return write_ ## SUFFIX ## _array(p, (const UBXTYPE*) x, len);	\
}                                                                       \
									\
long portWrite(const ubx_port_t *p, const CLASS* x)			\
{                                                                       \
	return write_ ## SUFFIX(p, (const UBXTYPE*) x);			\
}                                                                       \
									\
long configGet(const ubx_block_t *b, const char *cfg_name, const CLASS **valptr) \
{									\
	return cfg_getptr_ ## SUFFIX(b, cfg_name, (const UBXTYPE**) valptr); \
}									\

/*
 * The following macro define helper functions for working with C++
 * objects with struct reflection. This checks for (and errors) if the
 * class is not a standard_layout class.
 */

#define def_class_accessors(SUFFIX, CLASS, UBXTYPE) \
static_assert(std::is_standard_layout<CLASS>::value, QUOTE(CLASS) "has no standard layout"); \
                                                                        \
def_type_accessors(SUFFIX, UBXTYPE);                                    \
                                                                        \
gen_class_accessors(SUFFIX, CLASS, UBXTYPE);				\

#endif
