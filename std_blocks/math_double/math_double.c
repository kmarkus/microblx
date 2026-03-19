/*
 * math function block - shared implementation, parameterized by MATH_T.
 * Compiled multiple times (math_double, math_float) via -DMATH_T=...
 */

#include "math_block.h"

int parse_func(ubx_block_t *b, struct math_info *inf)
{
	long len;
	const char *func;

	len = cfg_getptr_char(b, CFUNC, &func);
	assert(len > 0);
	(void) len;

	for (unsigned int i=0; i<ARRAY_SIZE(functions); i++) {
		if (strncasecmp(func, functions[i].name, MATHFUNC_MAXLEN) == 0) {
			ubx_debug(b, "found math function %s", func);
			inf->func = &functions[i];
			return 0;
		}
	}
	ubx_err(b, "unsupported math function %s", func);
	return -1;
}

int math_init(ubx_block_t *b)
{
	int ret = -1;
	long len;
	const long *data_len;
	struct math_info *inf;

	inf = calloc(1, sizeof(struct math_info));

	if (inf == NULL) {
		ubx_err(b, "math: failed to alloc memory");
		return EOUTOFMEM;
	}

	b->private_data = inf;

	inf->p_x = ubx_port_get(b, "x");
	inf->p_y = ubx_port_get(b, "y");

	/* handle data_len conf */
	len = cfg_getptr_long(b, "data_len", &data_len);
	assert(len>=0);

	inf->data_len = (len > 0) ? *data_len : 1;

	/* resize ports */
	if (ubx_inport_resize(inf->p_x, inf->data_len) ||
	    ubx_outport_resize(inf->p_y, inf->data_len))
		return -1;

	ret = parse_func(b, inf);

	/* mul */
	len = MATH_CFG_GETPTR(b, CMUL, &inf->mul);
	assert(len>=0);

	if (len != 0 && len != inf->data_len) {
		ubx_err(b, "EINVALID_CONFIG_LEN of %s: expected %lu, got %lu",
			CMUL, inf->data_len, len);
		return EINVALID_CONFIG_LEN;
	}

	/* add */
	len = MATH_CFG_GETPTR(b, CADD, &inf->add);
	assert(len>=0);

	if (len != 0 && len != inf->data_len) {
		ubx_err(b, "EINVALID_CONFIG_LEN of %s: expected %lu, got %lu",
			CADD, inf->data_len, len);
		return EINVALID_CONFIG_LEN;
	}

	if (ret)
		return -1;

	return 0;
}

/* cleanup */
void math_cleanup(ubx_block_t *b)
{
	free(b->private_data);
}

/* step */
void math_step(ubx_block_t *b)
{
	long len;
	struct math_info *inf = (struct math_info *)b->private_data;

	MATH_T data[inf->data_len];

	len = MATH_READ_ARRAY(inf->p_x, data, inf->data_len);

	if (len < 0) {
		ubx_err(b, "error reading port x: %lu", len);
		return;
	} else if (len == 0) {
		ubx_notice(b, "unexpected NO_DATA on port x");
		return;
	} else if (len != inf->data_len) {
		ubx_err(b, "EINVALID_DATA_LEN: %lu", len);
		return;
	}

	for (long i=0; i<inf->data_len; i++)
		data[i] = (MATH_T) inf->func->f((double) data[i]);

	if (inf->mul){
		for (long i=0; i<inf->data_len; i++)
			data[i] *= inf->mul[i];
	}

	if (inf->add){
		for (long i=0; i<inf->data_len; i++)
			data[i] += inf->add[i];
	}

	MATH_WRITE_ARRAY(inf->p_y, data, inf->data_len);
}
