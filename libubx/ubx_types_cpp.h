#ifndef UBX_TYPES_CPP_H
#define UBX_TYPES_CPP_H

#include <inttypes.h>   /* uint32_t */
#include "ubx_types.h"

struct ubx_port_cpp : public ubx_port
{
	ubx_port_cpp (
		const char* _name,
		const char* _in_type_name,
		const char* _out_type_name,
		uint32_t _attrs)
	{
		ubx_port::name = _name;
		ubx_port::meta_data = NULL;
		ubx_port::attrs =_attrs;
		ubx_port::state = 0;
		ubx_port::in_type_name = _in_type_name;
		ubx_port::out_type_name = _out_type_name;
		ubx_port::in_type = NULL;
		ubx_port::out_type = NULL;
		ubx_port::in_data_len = 0;
		ubx_port::out_data_len = 0;
		ubx_port::in_interaction = NULL;
		ubx_port::out_interaction = NULL;
		ubx_port::stat_writes = 0;
		ubx_port::stat_reades = 0;
	};
};

struct ubx_config_cpp : public ubx_config
{
	ubx_config_cpp(
	const char* _name,
	const char* _meta_data,
	const char* _type_name)
	{
		name = _name;
		meta_data = _meta_data;
		type_name = _type_name;
		attrs = 0;
	};
};


struct ubx_block_cpp : public ubx_block
{
	ubx_block_cpp (
		const char* _name,
		uint32_t _type,
		const char* _meta_data,
		ubx_config_t* _configs,
		ubx_port_t* _ports,
		int(*_init) (struct ubx_block*),
		int(*_start) (struct ubx_block*),
		void(*_stop) (struct ubx_block*),
		void(*_step) (struct ubx_block*),
		void(*_cleanup) (struct ubx_block*))
	{
		name = _name;
		meta_data = _meta_data;
		type = _type;
		ports = _ports;
		configs = _configs;
		block_state = 0;
		prototype = NULL;
		ni = NULL;
		init = _init;
		start = _start;
		stop = _stop;
		step = _step;
		cleanup = _cleanup;
		private_data = NULL;
	};
};

#endif // UBX_TYPESCPP_H
