#ifndef UBX_TYPES_CPP_H
#define UBX_TYPES_CPP_H

#include <inttypes.h>   /* uint32_t */
#include "ubx_types.h"

struct ubx_port_cpp : public ubx_port
{
	ubx_port_cpp (
		const char* _name,
		const char* _in_type_name,
		unsigned long _in_data_len,
		const char* _out_type_name,
		unsigned long _out_data_len,
		const char* _doc)
	{
		ubx_port::name = _name;
		ubx_port::doc = _doc;
		ubx_port::attrs = 0;
		ubx_port::state = 0;
		ubx_port::in_type_name = _in_type_name;
		ubx_port::out_type_name = _out_type_name;
		ubx_port::in_type = NULL;
		ubx_port::out_type = NULL;
		ubx_port::in_data_len = _in_data_len;
		ubx_port::out_data_len = _out_data_len;
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
	const char* _type_name,
	const char* _doc)
	{
		name = _name;
		type_name = _type_name;
		doc = _doc;
		attrs = 0;
	};
};

struct ubx_type_cpp : public ubx_type
{
	ubx_type_cpp (
		const char* _name,
		uint32_t _type_class,
		unsigned long _size,
		const void* _private_data)
	{
		ubx_type::name = _name;
		ubx_type::type_class = _type_class;
		ubx_type::size = _size;
		ubx_type::private_data = _private_data;
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
