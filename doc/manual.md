Microblx User Manual
====================

This manual describes microblx in a cookbook style:

 1. Key concepts
 1. How to build a microblx block
 1. How to assemble blocks into a system and how to use std_blocks like the webinterface (`webif` block) or the logger (`file_logger`))


Key concepts
------------

- block: the main building block. Is defined by filling in a
  `ubx_block_t` type and registering it with a microblx `node`.
  
- node: an administrative entity into which




Building microblx blocks
------------------------

The following is based on the (heavily documented random number
generator block (`std_blocks/random/`).

Building a block entails the following:

1. declaring configuration
1. declaring ports
1. declaring types
1. declaring block meta-data
1. declaring/implementing operations
1. declaring the block
1. registration of blocks and types
1. autogeneration of blocks

### declaring configuration

```C
ubx_config_t rnd_config[] = {
	{ .name="min_max_config", .type_name = "struct random_config" },
	{ NULL },
};
```


### declaring ports

```C
ubx_port_t rnd_ports[] = {
	{ .name="seed", .attrs=PORT_DIR_IN, .in_type_name="unsigned int" },
	{ .name="rnd", .attrs=PORT_DIR_OUT, .out_type_name="unsigned int" },
	{ NULL },
};
```
### declaring block meta-data

```C
char rnd_meta[] =
	"{ doc='A random number generator function block',"
	"  license='LGPL',"
	"  real-time=true,"
	"}";
```

### declaring/implementing operations

```C
int rnd_init(ubx_block_t *b)
int rnd_start(ubx_block_t *b);
void rnd_stop(ubx_block_t *b);
void rnd_cleanup(ubx_block_t *b);
void rnd_step(ubx_block_t *b);
```

### declaring the block

```C
ubx_block_t random_comp = {
	.name = "random/random",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = rnd_meta,
	.configs = rnd_config,
	.ports = rnd_ports,

	/* ops */
	.init = rnd_init,
	.start = rnd_start,
	.step = rnd_step,
	.cleanup = rnd_cleanup,
};
```
### declaring types

### registration of blocks and types assembling blocks
### autogeneration of blocks

Assembling blocks
-----------------


Tips and Tricks
---------------

 - cRemember to make all functions that `static`
 - 
 
