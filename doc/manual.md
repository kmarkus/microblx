Microblx User Manual
====================

Microblx is a lightweight function block model and implementation.

This manual describes microblx in a cookbook style:

 1. Key concepts
 1. How to build a microblx block
 1. How to assemble blocks into a system and how to use std_blocks like the webinterface (`webif` block) or the logger (`file_logger`))


Key concepts
------------

- **block**: the main building block. Is defined by filling in a
  `ubx_block_t` type and registering it with a microblx
  `ubx_node_t`. Blocks _have_ configuration, ports and operations.
  
  There are two types of blocks: **computation blocks** ("cblocks",
  `BLOCK_TYPE_COMPUTATION`) encapsulate "functionality" such as
  drivers and controllers. **interaction blocks** ("iblocks",
  `BLOCK_TYPE_INTERACTION`) are used to implement communication or
  interaction between blocks. This manual focusses on how to build
  cblocks, since this is what most application builders need to do.

- **configuration**: defines static properties of blocks, such as
  control parameters, device file names etc.

- **port**: define which data flows in and out of blocks.

- **node**: an adminstrative entity which keeps track of blocks and
  types. Typically one per process is used, but there's no constraint
  whatsoever.


Building microblx blocks
------------------------

The following is based on the (heavily documented random number
generator block (`std_blocks/random/`).

Building a block entails the following:

1. declaring configuration: what is the static configuration of a block
1. declaring ports: what is the input/output of a block
1. declaring types: which data types are communicated
1. declaring block meta-data: provide further information about a block
1. declaring/implementing hook functions: how is the block initialized, started, run, stopped and cleanup'ed?
    1. reading configuration values: how to access configuration from inside the block
    1. reading and writing from ports: how to read and write from ports
1. declaring the block: how to put everything together
1. registration of blocks and types: make block prototypes and types known to the system
1. autogeneration of blocks: helper tools


### declaring configuration

Configuration is described with a `NULL` terminated array of
`ubx_config_t` types:

```C
ubx_config_t rnd_config[] = {
	{ .name="min_max_config", .type_name = "struct random_config" },
	{ NULL },
};
```

The above defines a single configuration called "min_max_config" of
the type "struct random_config".

**_Note:_**: custom types like `struct random_config` must be
  registered with the system. (see section "declaring types")


### declaring ports

Like configurations, ports are described with a `NULL` terminated
array of ubx_config_t types:

```C
ubx_port_t rnd_ports[] = {
	{ .name="seed", .in_type_name="unsigned int" },
	{ .name="rnd", .out_type_name="unsigned int" },
	{ NULL },
};
```

Depending on whether an `in_type_name`, an `out_type_name` or both are
defined, the port will be an in-, out- or a bidirectional port.


### declaring block meta-data

```C
char rnd_meta[] =
	"{ doc='A random number generator function block',"
	"  license='LGPL',"
	"  real-time=true,"
	"}";
```

Additional meta-data can be defined as shown above. The following
keys are supported so far:

- `doc:` short descriptive documentation of the block

- `license`: license of the block. This will be used by deployment
  tools to validate whether the resulting license are compatible.
  
- `real-time`: is the block real-time safe, i.e. there are is no
  memory allocation / deallocation and other non deterministic
  function calls in the `step` function.

### declaring/implementing operations

The following block operations can be implemented to realize the
blocks behavior. All are optional.

```C
int rnd_init(ubx_block_t *b);
int rnd_start(ubx_block_t *b);
void rnd_stop(ubx_block_t *b);
void rnd_cleanup(ubx_block_t *b);
void rnd_step(ubx_block_t *b);
```

![Block lifecycle FSM](./figures/life_cycle.png?raw=true)


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

All types used in configurations and ports must be declared and
registered. This is necessary because microblx needs to know the size
of the transported data. Moreoever, it enables type reflection which
is used by logging or the webinterface.

In the random block example, we used a `struct
random_config`, that is defined in `types/random_config.h`:

```C
struct random_config {
	int min;
	int max;
};
```

It can be declared as follows:

```C
#include "types/random_config.h"
#include "types/random_config.h.hexarr"
ubx_type_t random_config_type = def_struct_type(struct random_config, &random_config_h);
```

This fills in a `ubx_type_t random_config_type`, which stores the
information on a type. This type can then be registered with a node
(see "Block and type registration" below).

**What is this .hexarr file?**

`types/random_config.h.hexarr` is the contents of
`types/random_config.h` converted to an array `const char
random_config_h []` using the script `tools/file2carr.lua`. This char
array is stored in the `ubx_type_t private_data` field (the third
argument to the `def_struct_type` macro). At runtime, this type model
is loaded into the luajit ffi, thereby enabling type reflection
features such as logging or changing configuration values via the
webinterface. The conversion from `.h` to `.hexarray` is done via a
simple makefile rule.

This feature is optional. If no type reflection is needed, don't
include the `.hexarr` file and pass `NULL` as a third argument to
`def_struct_type`.


### Block and type registration

So far we have _declared_ blocks and types. To make them known to the
system, these need to be registered a microblx node. This is done in
the module init function, which is called when a module is loaded:

```C
1: static int rnd_module_init(ubx_node_info_t* ni)
2: {
3:	ubx_type_register(ni, &random_config_type);
4: 	return ubx_block_register(ni, &random_comp);
5: }
6: UBX_MODULE_INIT(rnd_module_init)
```


### assembling blocks

### block autogeneration

The script `tools/ubx_genblock.lua` generates a dummy microblx block
including makefile.

Example: generate stubs for a `mul` block:

```bash
$ tools/ubx_genblock.lua -n mul -d std_blocks/math
```

Assembling blocks
-----------------


Tips and Tricks
---------------

### Compiling C++

Currently only works with `clang`, because g++ does not support
designated initializers (yet).

### speeding up port writing


### Module visibility

The default Makefile defines `-fvisibility=hidden`, so there's no need
to prepend functions and global variables with `static`
