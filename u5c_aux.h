
/* normally the user would have to box/unbox his value himself. This
 * would generate a typed, automatic boxing version for
 * convenience. */
#define gen_write(function_name, typename) \
void function_name(u5c_port* port, typename *outval) \
{ \
 u5c_value val; \
 val.data = outval; \
 val.type = port->data_type_id; \
 port->write(&val); \
} \

/* generate a typed read function: arguments to the function are the
 * port and a pointer to the result value. 
 */
#define gen_read(function_name, typename) \
uint32_t function_name(u5c_port* port, typename *inval) \
{ \
 u5c_value val; \
 val.data = inval;	  \
 return port->read(&val); \
} \
