Microblx User Manual
====================

Microblx is a lightweight function block model and implementation.

<!-- markdown-toc start - Don't edit this section. Run M-x markdown-toc-generate-toc again -->
**Table of Contents**

- [Microblx User Manual](#microblx-user-manual)
    - [Key concepts](#key-concepts)
    - [Building microblx blocks](#building-microblx-blocks)
        - [Declaring configuration](#declaring-configuration)
        - [Declaring ports](#declaring-ports)
        - [Declaring block meta-data](#declaring-block-meta-data)
        - [Declaring/implementing block hook functions](#declaringimplementing-block-hook-functions)
            - [Storing block local state](#storing-block-local-state)
            - [Reading configuration values](#reading-configuration-values)
            - [When to read configuration: init vs start?](#when-to-read-configuration-init-vs-start)
            - [Reading from and writing to ports](#reading-from-and-writing-to-ports)
        - [Declaring the block](#declaring-the-block)
        - [Declaring types](#declaring-types)
        - [Block and type registration](#block-and-type-registration)
        - [Using real-time logging](#using-real-time-logging)
        - [SPDX License Identifier](#spdx-license-identifier)
        - [Block code-generation](#block-code-generation)
    - [Assembling blocks](#assembling-blocks)
    - [Tips and Tricks](#tips-and-tricks)
        - [Using C++](#using-c)
        - [Avoiding Lua scripting](#avoiding-lua-scripting)
        - [Speeding up port writing](#speeding-up-port-writing)
        - [What the difference between block types and instances?](#what-the-difference-between-block-types-and-instances)
        - [Module visibility](#module-visibility)

<!-- markdown-toc end -->


Key concepts
------------

- **block**: the main building block. Is defined by filling in a
  `ubx_block_t` type and registering it with a microblx
  `ubx_node_t`. Blocks _have_ configuration, ports and operations.

  Each block is part of a module and becomes available once the module
  is loaded in a node.

  There are two types of blocks: **computation blocks** ("cblocks",
  `BLOCK_TYPE_COMPUTATION`) encapsulate "functionality" such as
  drivers and controllers. **interaction blocks** ("iblocks",
  `BLOCK_TYPE_INTERACTION`) are used to implement communication or
  interaction between blocks. This manual focusses on how to build
  cblocks, since this is what most application builders need to do.

     - **configuration**: defines static properties of blocks, such
		  as control parameters, device file names etc.

     - **port**: define which data flows in and out of blocks.
		
- **type**: types of data sent through ports or of configuration must
    be registed with microblx. 

- **node**: an adminstrative entity which keeps track of blocks and
  types. Typically one per process is used, but there's no constraint
  whatsoever.
  
- **module**: a module contains one or more blocks or types that are
  registered/deregistered with a node when the module is
  loaded/unloaded.


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


### Declaring configuration

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


### Declaring ports

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


### Declaring block meta-data

```C
char rnd_meta[] =
	"{ doc='A random number generator function block',"
	"  realtime=true,"
	"}";
```

Additional meta-data can be defined as shown above. The following
keys are supported so far:

- `doc:` short descriptive documentation of the block

- `realtime`: is the block real-time safe, i.e. there are is no
  memory allocation / deallocation and other non deterministic
  function calls in the `step` function.

### Declaring/implementing block hook functions

The following block operations can be implemented to realize the
blocks behavior. All are optional.

```C
int rnd_init(ubx_block_t *b);
int rnd_start(ubx_block_t *b);
void rnd_stop(ubx_block_t *b);
void rnd_cleanup(ubx_block_t *b);
void rnd_step(ubx_block_t *b);
```

These functions can be called according to the microblx block
life-cycle finite state machine:

![Block lifecycle FSM](figures/life_cycle.png?raw=true)

They are typically used for the following:

- `init`: initialize the block, allocate memory, drivers: check if the
   device is there and return non-zero if not.
- `start`: become operational, open device, last checks. Cache
   pointers to ports, read configuration.
- `step`: read from ports, compute, write to ports
- `stop`: stop/close device. (often not used).
- `cleanup`: free all memory, release all resources.


#### Storing block local state

As multiple instances of a block may exists, **NO** global variables
may be used to store the state of a block. Instead, the `ubx_block_t`
defines a `void* private_data` pointer which can be used to store
local information. Allocate this in the `init` hook:

```C
if ((b->private_data = calloc(1, sizeof(struct random_info)))==NULL) {
	ubx_err(b, "Failed to alloc memory");
	goto out_err;
}

```

and retrieve it in the other hooks:

```C
struct block_info inf*;

inf = (struct random_info*) b->private_data;
```

#### Reading configuration values

The following example from the `random` block shows how to retrieve a
struct configuration called `min_max_config`:

```C
long int len;
struct random_config* rndconf;

/*...*/

if((len = ubx_config_get_data_ptr(b, "min_max_config", &rndconf)) < 0)
	goto err;

if(len==0)
	/* set a default or fail */
```

`ubx_config_get_data_ptr` returns the pointer to the actual
data. `len` will be set to the array lenghth: 0 if unconfigured, >0 if
configured and <0 in case of error.

For basic types there are several predefined and somewhat type safe
convenince functions `cfg_getptr_*`. For example, to retrieve a scalar
`uint32_t` and to use a default 47 if unconfigured:

```C
long int len;
uint32_t *value;

if ((len = cfg_getptr_int(b, "myconfig", &value)) < 0)
	goto out_err;

value = (len > 0) ? *value : 47;
```

#### When to read configuration: init vs start?

It depends: if needed for initalization (e.g. a char array describing
which device file to open), then read in `init`. If it's not needed in
`init` (e.g. like the random min-max values in the random block
example), then read it in start.

This choice affects reconfiguration: in the first case the block has
to be reconfigured by a `stop`, `cleanup`, `init`, `start` sequence,
while in the latter case only a `stop`, `start` sequence is necessary.

#### Reading from and writing to ports

The following helper macros are available to support

```C
def_read_fun(read_uint, unsigned int)
def_write_fun(write_uint, unsigned int)
```

### Declaring the block

The block aggregates all of the previous declarations into a single
data-structure that can then be registered in a microblx module:

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

### Declaring types

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

This fills in a `ubx_type_t` data structure called
`random_config_type`, which stores information on types. Using this
type declaration the `struct random_config` can then be registered
with a node (see "Block and type registration" below).

**What is this .hexarr file?**

The file `types/random_config.h.hexarr` contains the contents of the
file `types/random_config.h` converted to an array `const char
random_config_h []` using the tool `tools/ubx_tocarr`. This char
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
system, these need to be _registered_ when the respective _module_ is
loaded in a microblx node. This is done in the module init function,
which is called when a module is loaded:

```C
1: static int rnd_module_init(ubx_node_info_t* ni)
2: {
3:        ubx_type_register(ni, &random_config_type);
4:        return ubx_block_register(ni, &random_comp);
5: }
6: UBX_MODULE_INIT(rnd_module_init)
```

Line 3 and 4 register the type and block respectively. Line 6 tells
microblx that `rnd_module_init` is the module's init function.

Likewise, the module's cleanup function should deregister all types
and blocks registered in init:

```C
static void rnd_module_cleanup(ubx_node_info_t *ni)
{
	ubx_type_unregister(ni, "struct random_config");
	ubx_block_unregister(ni, "random/random");
}
UBX_MODULE_CLEANUP(rnd_module_cleanup)
```

### Using real-time logging

Microblx provides logging infrastructure with loglevels similar to the
Linux Kernel. Loglevel can be set on the (global) node level (e.g. by
passing it `-loglevel N` to `ubx_launch` or be overridden on a per
block basis. To do the latter, a block must define and configure a
`loglevel` config of type `int`. If it is left unconfigured, again the
node loglevel will be used.

The following loglevels are supported:

- `UBX_LOGLEVEL_EMERG`  (0) (system unusable)
- `UBX_LOGLEVEL_ALERT`  (1)	(immediate action required)
- `UBX_LOGLEVEL_CRIT`   (2)	(critical)
- `UBX_LOGLEVEL_ERROR`  (3)	(error)
- `UBX_LOGLEVEL_WARN`   (4)	(warning conditions)
- `UBX_LOGLEVEL_NOTICE` (5)	(normal but significant)
- `UBX_LOGLEVEL_INFO`   (6)	(info message)
- `UBX_LOGLEVEL_DEBUG`  (7)	(debug messages)

The following macros are available for logging from within blocks:

```C
ubx_emerg(b, fmt, ...)
ubx_alert(b, fmt, ...)
ubx_crit(b, fmt, ...)
ubx_err(b, fmt, ...)
ubx_warn(b, fmt, ...)
ubx_notice(b, fmt, ...)
ubx_info(b, fmt, ...)
ubx_debug(b, fmt, ...)
```

Note that `ubx_debug` will only be logged if `UBX_DEBUG` is defined in
the respective block and otherwise compiled out without any overhead.

To view the logmessages, you need to run the `ubx_log` tool in a
separate window.

**Important**: The maximum total log message length (including is by
default set to 80 by default), so make sure to keep log message short
and sweet (or increase the lenghth for your build).

Note that the old (non-rt) macros `ERR`, `ERR2`, `MSG` and `DBG` are
deprecated and shall not be used anymore.

Outside of the block context, (e.g. in `module_init` or
`module_cleanup`, you can log with the lowlevel function

```C
ubx_log(int level,
	    ubx_node_info_t *ni,
		const char* src,
		const char* fmt, ...)

/* for example */
ubx_log(UBX_LOGLEVEL_ERROR, ni, __FUNCTION__, "error %u", x);
```

e.g.

The ubx core uses the same logger, but mechanism, but uses the
`log_info` resp `logf_info` variants. See `libubx/ubx.c` for examples.

### SPDX License Identifier

Microblx uses a macro to define module licenses in a form that is both
machine readable and available at runtime:

```C
UBX_MODULE_LICENSE_SPDX(MPL-2.0)
```

To dual-license a block, write:

```C
UBX_MODULE_LICENSE_SPDX(MPL-2.0 BSD-3-Clause)
```

Is is strongly recommended to use this macro. The list of licenses can
be found here:

http://spdx.org/licenses/
http://spdx.org

(Credit: inspired by U-Boot).

### Block code-generation

The script `tools/ubx_genblock.lua` generates a microblx block
including a makefile. After this, only the hook functions need to be
implemented in the `.c` file:

Example: generate stubs for a `myblock` block (see
`examples/block_model_example.lua` for the block generator model).

```bash
$ tools/ubx_genblock.lua -c examples/block_model_example.lua -d std_blocks/test
    generating std_blocks/test/Makefile
    generating std_blocks/test/myblock.h
	generating std_blocks/test/myblock.c
	generating std_blocks/test/myblock.usc
	generating std_blocks/test/types/vector.h
	generating std_blocks/test/types/robot_data.h
```

If the command is run again, only the `.c` file will be NOT
regenerated. This can be overriden using the `-force` option.

Assembling blocks
-----------------

There are different options how to create a system from blocks:

 - by using a declarative description of the desired composition (the
   `*.usc` files under examples). `usc` stands for microblx system
   composition). These can be launched using the `ubx_launch` tool, e.g.

```bash
$ tools/ubx_launch -webif -c examples/trig_rnd_hexdump.usc
```

   this will launch␇the given system composition and additionally
   create a webserver block to allow system to be introspected.

 - by writing a so called "deployment script" (e.g. see
   `examples/trig_rnd_to_hexdump.lua`)

 - by assembling the necessary parts in C.


Tips and Tricks
---------------

### Using C++

See `std_blocks/cppdemo`. If the designated initalizers (the struct
initalization used in this manual) are used, the block must be
compiled with `clang`, because g++ does not support designated
initializers (yet).

### Avoiding Lua scripting

It is possible to avoid the Lua scripting layer entirely. A small
example can be found in `examples/c-only.c`. 
See also the [tutorial](tutorial.md) for a more complete example.

### Speeding up port writing

To speed up port writing, the pointers to ports can be cached in the
block info structure. The `ubx_genblock` script automatically takes
care of this.

### What the difference between block types and instances?

First: to create a block instance, it is cloned from an existing block
and the `block->prototype` char ponter set to a newly allocated string
holding the protoblocks name.

There's very little difference between prototypes and instances:

- a block type's `prototype` (char) ptr is `NULL`, while an instance's
  points to a (copy) of the prototype's name.
  
- Only block instances can be deregistered and freed (`ubx_block_rm`),
  prototypes must be deregistered (and freed if necessary) by the
  module's cleanup function.

### Module visibility

The default Makefile defines `-fvisibility=hidden`, so there's no need
to prepend functions and global variables with `static`
