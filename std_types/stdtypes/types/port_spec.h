#ifndef UBX_PORT_SPEC_H
#define UBX_PORT_SPEC_H

/**
 * struct ubx_port_spec
 *
 * describes a port that a block creates from its configuration
 *
 * @id: port name
 * @type: ubx type name of the port
 * @len: array length (0 means 1)
 */
struct ubx_port_spec {
	char id[UBX_PORT_NAME_MAXLEN + 1];
	char type[UBX_TYPE_NAME_MAXLEN + 1];
	uint32_t len;
};

#endif /* UBX_PORT_SPEC_H */
