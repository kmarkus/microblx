![microblx logo](docs/img/microblx-logo.svg)

microblx: hard real-time function blocks
========================================

[![pipeline status](https://gitlab.com/kmarkus/microblx/badges/master/pipeline.svg)](https://gitlab.com/kmarkus/microblx/-/pipelines)

Microblx is a lightweight and hard real-time safe function block
framework for use-cases such as *embedded control* or *signal
processing*. It provides generic and configurable code for typical
challenges such as *lock-free communication* between different
criticality domains, real-time safe logging or triggers with
configurable POSIX realtime properties.

- **hard real-time safe**: no run-time allocations or non-deterministic system calls
- **simple model**: everything is a block
- **extensible**: easy to add a new type, trigger block or connection
- **composing applications**: applications are described using a simple textual language
- **standard blocks and tools** included:
  - *communication*: lock free buffers, latest-value stores, POSIX
    message queues,
  - *computations*: PID controller, filters (moving average, EWMA),
    statistics, mux/demux, ramps, constants, random ...
  - *triggers*: periodic, passive incl. built in execution time and
    trigger latency profiling
  - real-time safe logging
- **no HAL**: no arbitrary abstractions, just "configurable" POSIX
- **minimal**: tiny memory footprint, few dependencies, embedded-friendly

<!-- markdown-toc start - Don't edit this section. Run M-x markdown-toc-refresh-toc -->
**Table of Contents**

- [Installing](#installing)
    - [Dependencies](#dependencies)
    - [Building](#building)
    - [Build options](#build-options)
    - [Yocto](#yocto)
- [Quickstart](#quickstart)
- [Concepts](#concepts)
- [Developing blocks](#developing-blocks)
    - [Generating a block](#generating-a-block)
    - [Configs](#configs)
    - [Ports](#ports)
    - [Meta-data](#meta-data)
    - [Hooks and life cycle](#hooks-and-life-cycle)
    - [Block local state](#block-local-state)
    - [Reading configs](#reading-configs)
    - [Reading and writing ports](#reading-and-writing-ports)
    - [Declaring the block](#declaring-the-block)
    - [Types](#types)
    - [Module registration](#module-registration)
    - [Logging](#logging)
    - [Guidelines](#guidelines)
    - [C++ and Lua blocks](#c-and-lua-blocks)
- [Composing systems](#composing-systems)
    - [Launching from C](#launching-from-c)
- [Tools](#tools)
- [Standard blocks](#standard-blocks)
- [Tracing](#tracing)
- [Lua API docs](#lua-api-docs)
- [FAQ](#faq)
    - [Running](#running)
    - [Debugging](#debugging)
    - [Developing](#developing)
- [Getting help](#getting-help)
- [Related projects](#related-projects)
- [Contributing](#contributing)
- [License](#license)
- [Acknowledgement](#acknowledgement)

<!-- markdown-toc end -->

Installing
----------

### Dependencies

Mandatory (Debian/Ubuntu):

```sh
apt install cmake pkg-config luajit libluajit-5.1-dev uthash-dev \
    libsystemd-dev libmxml-dev
```

Mandatory, from source:
[uutils](https://github.com/kmarkus/uutils),
[ffi-reflect](https://github.com/corsix/ffi-reflect) and
[optparse](https://github.com/gvvaughan/optparse) (for `ubx-launch`
and `ubx-dbus`, alternatively `luarocks install optparse`):

```sh
git clone --depth=1 https://github.com/kmarkus/uutils.git
git clone --depth=1 https://github.com/corsix/ffi-reflect.git
git clone --depth=1 --branch v1.5 https://github.com/gvvaughan/optparse.git
cd uutils && sudo make install && cd ..
sudo install -d /usr/local/share/lua/5.1/optparse/
sudo cp ffi-reflect/reflect.lua /usr/local/share/lua/5.1/
sudo cp optparse/lib/optparse/*.lua /usr/local/share/lua/5.1/optparse/
```

Optional blocks are built if their dependencies are found:

| block                    | dependency           | install                                |
|--------------------------|----------------------|----------------------------------------|
| `lsdb-intf`              | lsdbus (from source) | see below, and `cmake -DBLOCK_LSDB_INTF=ON` |
| `webgraph`               | lua-socket, json.lua | `apt install lua-socket lua-json`      |
| `ubx/gps`                | libgps               | `apt install libgps-dev gpsd`          |
| `ubx/gpio`               | libgpiod >= 2.0      | `apt install libgpiod-dev`             |
| `ubx/iio`, `ubx/iio_buf` | libiio >= 0.21       | `apt install libiio-dev libiio-utils`  |
| `ubx-log -d` (daemon)    | libdaemon            | `apt install libdaemon-dev`            |
| tests                    | luaunit              | `apt install lua-unit`                 |

```sh
git clone https://github.com/kmarkus/lsdbus.git
cd lsdbus && mkdir build && cd build
cmake .. -DCONFIG_LUA_VER=jit
make -j$(nproc) && sudo make install
```

### Building

```sh
git clone https://gitlab.com/kmarkus/microblx.git
cd microblx && mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install && sudo ldconfig
```

### Build options

| option                          | description                                                       |
|---------------------------------|-------------------------------------------------------------------|
| `-DENABLE_TIMESRC_TSC=ON`       | x86 TSC as time source; assumes a fixed CPU frequency `-DCPU_HZ=<hz>` |
| `-DENABLE_TIMESRC_CNTVCT=ON`    | aarch64 `CNTVCT` counter as time source (excludes TSC)            |
| `-DTRACING=SDT\|MARKER`         | tracing instrumentation, see [Tracing](#tracing)                  |
| `-DUBX_LOG_MSG_MAXLEN=<n>`      | max log message length; part of the log shm layout, writers and `ubx-log` must agree |
| `-DUBX_LOG_DAEMON=AUTO\|ON\|OFF` | `ubx-log` daemon mode (`-d`), requires libdaemon                 |
| `-DBUILD_LUA_DOCS=ON`           | Lua API docs, see [Lua API docs](#lua-api-docs)                   |
| `-DCMAKE_BUILD_TYPE=<type>`     | default `RelWithDebInfo`                                          |

Timing-sensitive systems should enable a hardware time source. The
default POSIX clock costs a `clock_gettime` per timestamp, the counter
sources a register read. On a 1.25 GHz Cortex-A53 (gcc 13.4 `-O2`):

| time source                        | per read |
|------------------------------------|----------|
| POSIX (`clock_gettime`, vDSO)      | 65.3 ns  |
| `CNTVCT`                           | 15.2 ns  |

This is paid per timestamp: a `ptrig` with `tstats_mode=2` over a
four-block chain takes eleven per cycle (~0.7 us POSIX vs. ~0.2 us
`CNTVCT`, charged to the measured step duration). It also sets the
resolution of the busy-wait in `sleep_mode` 1 and 2 (~68 ns POSIX vs.
~9 ns `CNTVCT`).

Query the options of an installation (also `ubx-modinfo -version`;
timesource and tracing backend are logged at `INFO` on node init):

```sh
$ ubx-launch --version
microblx v1.0.0-rc3-4-ge7ba646 (modver 0.9)
build options:
  timesrc:             POSIX
  tracing:             OFF
  log_msg_maxlen:      115
  build_type:          RelWithDebInfo
  compiler:            GNU 16.2.0
  arch:                x86_64
  sched_attr:          yes
  sched_dl_overrun:    yes
  pthread_setname:     yes
  pthread_setaffinity: yes
  module_dir:          /usr/local/lib/ubx/0.9/
```

### Yocto

For embedded targets use the
[meta-microblx](https://github.com/kmarkus/meta-microblx) layer.

Quickstart
----------

Start the log client in a separate terminal:

```sh
$ ubx-log
waiting for rtlog.logshm to appear
```

Launch the [threshold](examples/usc/threshold.usc) example: a ramp
feeds a sine generator whose output is checked against a threshold.
Crossing events go to an mqueue:

```
/------\    /-----\    /-----\    /----\
| ramp |--->| sin |--->|thres|--->| mq |
\------/    \-----/    \-----/    \----/
   ^           ^          ^
   .           . #2       .
#1 .           .          .
   .        /------\      . #3
   .........| trig |.......
            \------/

---> data flow
...> triggers
```

```sh
ubx-launch -l 7 -c /usr/local/share/ubx/examples/usc/threshold.usc
```

Dump the events (Ctrl-C stops `ubx-launch`):

```sh
ubx-mq list
ubx-mq read thres.event -p threshold
```

A larger example: a PID controller composed from several usc files,
with a non-RT trigger mixed in and a live graph at
<http://localhost:8888>:

```sh
cd /usr/local/share/ubx/examples/usc/pid/
ubx-launch --webgraph -c pid_test.usc,ptrig_nrt.usc
```

```sh
$ ubx-mq list
$ ubx-mq read controller_pid-out
```

Concepts
--------

- **block**: has *configs* (static configuration), *ports* (data in
  and out) and *hooks* called by the [life cycle](#hooks-and-life-cycle).
- **cblock** (computation block): the regular functional block with a
  `step` hook.
- **iblock** (interaction block): implements `read` and `write` to
  connect cblocks, e.g. lock-free buffers or mqueues. The standard
  iblocks cover most needs.
- **trigger**: a cblock that steps a configured chain of blocks, e.g.
  `ubx/ptrig` (periodic pthread). Custom triggers (e.g. on external
  events) use `libubx/trig_utils.h`.
- **types**: C types (primitives, structs, arrays of both) for configs
  and port data. Custom types must be registered to be usable in usc
  files and tools. `stdtypes` provides the common ones (`int`,
  `double`, `int32_t`, ..., `struct ubx_tstat`).
- **module**: shared library containing blocks and/or types, loaded at
  launch.
- **node**: run-time container into which modules are loaded and
  which holds the block instances.
- **dynamic interface**: port type or length depends on configs,
  e.g. iblocks use the canonical configs `type_name` and `data_len`.
- **usc**: the [composition language](#composing-systems) describing
  an application.

Developing blocks
-----------------

The snippets below are from the
[random](std_blocks/examples/random/random.c) example block. A block
needs configs, ports, hooks, a block declaration and a module init
function that registers it. [skelleton](std_blocks/skelleton/) is an
annotated template.

### Generating a block

`ubx-genblock` generates a block with build files from a block model
([example](examples/blockmodels/block_model_example.lua)). Only the
hooks in the `.c` file need to be implemented:

```sh
$ ubx-genblock -d myblock -c /usr/local/share/ubx/examples/blockmodels/block_model_example.lua
    generating myblock/bootstrap
    generating myblock/configure.ac
    generating myblock/Makefile.am
    generating myblock/myblock.h
    generating myblock/myblock.c
    generating myblock/myblock.usc
    generating myblock/types/vector.h
    generating myblock/types/robot_data.h
```

| file              | content                                          |
|-------------------|--------------------------------------------------|
| `myblock.h`       | interface and module registration (don't edit)   |
| `myblock.c`       | hooks: edit and implement                        |
| `myblock.usc`     | composition to run the block                     |
| `types/*.h`       | sample types: fill in the struct bodies          |

Rerunning regenerates everything but the `.c` file (override with
`-force`). `-cpp` generates a C++ block. Build and run:

```sh
cd myblock/
./bootstrap && ./configure && make && sudo make install
ubx-launch --webgraph -c myblock.usc
```

### Configs

A `{ 0 }` terminated array of `ubx_proto_config_t`:

```c
ubx_proto_config_t rnd_config[] = {
	{ .name = "loglevel", .type_name = "int" },
	{ .name = "min_max_config", .type_name = "struct random_config", .min = 1, .max = 1 },
	{ 0 },
};
```

`min` and `max` constrain the array length, checked before `init`
(before `start` with `.attrs = CONFIG_ATTR_CHECKLATE`):

| min | max              | result                  |
|-----|------------------|-------------------------|
| 0   | 0 or unset       | no checking             |
| 0   | 1                | optional                |
| 1   | 1                | mandatory               |
| 0   | `CONFIG_LEN_MAX` | zero to many            |
| N   | M                | between N and M         |

Static definitions use the `ubx_proto_*` types, hooks the runtime
types (`ubx_config_t`, `ubx_port_t`, `ubx_block_t`).

### Ports

A `{ 0 }` terminated array of `ubx_proto_port_t`. `in_type_name`,
`out_type_name` or both make an in-, out- or in/out port:

```c
ubx_proto_port_t rnd_ports[] = {
	{ .name = "seed", .in_type_name = "unsigned int" },
	{ .name = "rnd", .out_type_name = "unsigned int" },
	{ 0 },
};
```

### Meta-data

```c
char rnd_meta[] =
	"{ doc='A random number generator function block',"
	"  realtime=true,"
	"}";
```

- `doc`: short description
- `realtime`: `step` is real-time safe (no allocations or other
  non-deterministic calls)

### Hooks and life cycle

All hooks are optional:

```c
int  rnd_preinit(ubx_block_t *b);
int  rnd_init(ubx_block_t *b);
int  rnd_start(ubx_block_t *b);
void rnd_step(ubx_block_t *b);
void rnd_stop(ubx_block_t *b);
void rnd_cleanup(ubx_block_t *b);
void rnd_preexit(ubx_block_t *b);
```

![block life cycle FSM](docs/img/life_cycle.svg)

| hook      | typical use                                                                         |
|-----------|-------------------------------------------------------------------------------------|
| `preinit` | extend the interface (add/resize ports, create configs) from static config values   |
| `init`    | allocate memory and resources, open the device, validate configs. Return 0 if OK    |
| `start`   | become operational: enable the device, cache port pointers, apply runtime configs   |
| `step`    | read ports, compute, write ports                                                    |
| `stop`    | disable the device (rarely used)                                                    |
| `cleanup` | free everything allocated in `init`                                                 |
| `preexit` | free private data allocated in `preinit` (ports and configs are freed by the framework) |

`preinit` and `init` both run in state `preinit` and may change the
interface. The deployment applies configs between them (see the
[launch sequence](docs/usc.md#launch-sequence)): `preinit` sees only
the static configs, `init` also the configs created by `preinit`.
`ubx_block_init` runs `preinit` automatically. Blocks with a fixed
interface only need `init`..`cleanup`.

### Block local state

No globals: a block type can have many instances. Use
`b->private_data`:

```c
struct random_info {
	unsigned int min;
	unsigned int max;
};

int rnd_init(ubx_block_t *b)
{
	b->private_data = calloc(1, sizeof(struct random_info));

	if (b->private_data == NULL) {
		ubx_crit(b, "ENOMEM");
		return EOUTOFMEM;
	}
	return 0;
}

void rnd_cleanup(ubx_block_t *b)
{
	free(b->private_data);
}
```

### Reading configs

`cfg_getptr_<TYPE>` returns <0 on error, 0 if unconfigured, else the
array length, and points `val` to the data:

```c
long len;
const int *val;

if ((len = cfg_getptr_int(b, "myconfig", &val)) < 0)
	return -1;

int myconfig = (len > 0) ? *val : 47;	/* default 47 */
```

For custom types, define the accessor with a
[type macro](#type-safe-accessors):

```c
def_cfg_getptr_fun(cfg_getptr_random_config, struct random_config)

int rnd_start(ubx_block_t *b)
{
	long len;
	const struct random_config *rndconf;
	struct random_info *inf = b->private_data;

	len = cfg_getptr_random_config(b, "min_max_config", &rndconf);

	if (len < 0) {
		ubx_err(b, "failed to retrieve min_max_config");
		return -1;
	} else if (len == 0) {
		inf->min = 0;
		inf->max = INT_MAX;
	} else {
		inf->min = rndconf->min;
		inf->max = rndconf->max;
	}
	return 0;
}
```

Copying to `private_data` is only needed for defaults; otherwise use
the pointer directly. Permitted config changes per state:

| block state | allowed config changes |
|-------------|------------------------|
| `preinit`   | resize and change      |
| `inactive`  | change values          |
| `active`    | none                   |

Configs may be resized in `preinit`, so re-retrieve pointer and length
in `init`.

**init or start?** Read configs needed for initialization (e.g. a
device file) in `init`, others in `start`. Reconfiguring the former
takes `stop`, `cleanup`, `init`, `start`, the latter only `stop`,
`start`.

### Reading and writing ports

`read_<TYPE>` returns <0 on error, 0 if no data, else the array
length:

```c
ubx_port_t *p_rnd = ubx_port_get(b, "rnd");	/* cache in start */

unsigned int val = 1;
write_uint(p_rnd, &val);

long len;
int in;

len = read_int(p_in, &in);

if (len < 0)
	ubx_err(b, "port read failed");
else if (len == 0)
	;	/* no data */
else
	ubx_info(b, "new data: %i", in);
```

`read_<TYPE>_array` and `write_<TYPE>_array` handle arrays. Accessors
for all basic types are in `<ubx.h>`, for custom types see
[type safe accessors](#type-safe-accessors). Example:
[ramp](std_blocks/ramp/ramp.c).

### Declaring the block

```c
ubx_proto_block_t random_comp = {
	.name = "ubx/random",
	.meta_data = rnd_meta,
	.type = BLOCK_TYPE_COMPUTATION,	/* or BLOCK_TYPE_INTERACTION */

	.ports = rnd_ports,
	.configs = rnd_config,

	.init = rnd_init,
	.start = rnd_start,
	.cleanup = rnd_cleanup,
	.step = rnd_step,
};
```

Optional `.attrs`:

| attribute            | meaning                     |
|----------------------|-----------------------------|
| `BLOCK_ATTR_ACTIVE`  | block runs its own thread   |
| `BLOCK_ATTR_TRIGGER` | block steps other blocks    |

`ubx-launch` starts active blocks last, the D-Bus `ClearNode` stops
trigger blocks before removing anything. A block that steps other
blocks (e.g. via a `struct ubx_triggee` chain) **must** declare
`BLOCK_ATTR_TRIGGER`, otherwise it may step blocks that are being
removed.

### Types

Config and port types must be registered: microblx needs their size,
and the header enables reflection (usc configs, `ubx-mq`, logging).

```c
/* types/random_config.h */
struct random_config {
	unsigned int min;
	unsigned int max;
};
```

```c
#include "types/random_config.h"
#include "types/random_config.h.hexarr"

ubx_type_t random_config_type = def_struct_type(struct random_config, &random_config_h);
```

The `.hexarr` is the header as a C char array (`random_config_h`),
generated by `tools/ubx-tocarr` in the build
([CMake](std_blocks/examples/random/CMakeLists.txt): `generate_hexarr`).
At runtime it is loaded into the LuaJIT FFI. Without reflection, pass
`NULL` instead.

Rules for type headers (they are passed to `ffi.cdef`):

1. no `#include`: the C preprocessor is not run. Only use types the
   FFI knows (all builtins and `<stdint.h>` types).
2. one registered struct per header. Enums and unions used only by
   that struct may be in the same header.

Supported constructs:

```c
/* named enum field: Lua sees a number, usc configs also accept "BLUE" */
enum test_color { RED=0, GREEN=1, BLUE=2 };
struct test_with_enum { enum test_color col; int val; };

/* named union field: converted to a table of all members */
union test_variant { int i; float f; };
struct test_with_union { union test_variant v; unsigned char tag; };

/* anonymous union: members promoted to the struct, { i=42, selector=0 } */
struct test_with_anon_union { union { int i; float f; }; unsigned char selector; };

/* anonymous enum field: { kind="KIND_FLOAT", value=3 } */
struct test_with_anon_enum { enum { KIND_INT=0, KIND_FLOAT=1 } kind; int value; };
```

Custom conversion to Lua for a named struct or union (key includes
`struct`/`union`; anonymous ones can't be hooked, hook the containing
struct):

```lua
local cdata = require("cdata")

cdata.struct2tab["struct test_with_enum"] = function(cd)
   local names = { [0]="RED", [1]="GREEN", [2]="BLUE" }
   return { col = names[tonumber(cd.col)], val = tonumber(cd.val) }
end

-- expose only the active union member
cdata.struct2tab["union test_variant"] = function(cd) return tonumber(cd.i) end
```

#### Type safe accessors

```c
def_type_accessors(SUFFIX, TYPENAME)

/* defines */
long read_SUFFIX(const ubx_port_t *p, TYPENAME *val);
int write_SUFFIX(const ubx_port_t *p, const TYPENAME *val);
long read_SUFFIX_array(const ubx_port_t *p, TYPENAME *val, const int len);
int write_SUFFIX_array(const ubx_port_t *p, const TYPENAME *val, const int len);
long cfg_getptr_SUFFIX(const ubx_block_t *b, const char *cfg_name, const TYPENAME **valptr);
```

| macro                                 | defines                    |
|---------------------------------------|----------------------------|
| `def_type_accessors(SUFFIX, TYPE)`    | port and config accessors  |
| `def_port_accessors(SUFFIX, TYPE)`    | port accessors             |
| `def_port_readers(FUNCNAME, TYPE)`    | port read accessors        |
| `def_port_writers(FUNCNAME, TYPE)`    | port write accessors       |
| `def_cfg_getptr_fun(FUNCNAME, TYPE)`  | config getter              |
| `def_cfg_set_fun(FUNCNAME, TYPE)`     | config setter (launching from C) |

### Module registration

```c
int rnd_module_init(ubx_node_t *nd)
{
	if (ubx_type_register(nd, &random_config_type))
		return -1;
	return ubx_block_register(nd, &random_comp);
}

void rnd_module_cleanup(ubx_node_t *nd)
{
	ubx_type_unregister(nd, "struct random_config");
	ubx_block_unregister(nd, "ubx/random");
}

UBX_MODULE_INIT(rnd_module_init)
UBX_MODULE_CLEANUP(rnd_module_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
```

The license is an [SPDX](https://spdx.org/licenses) identifier,
dual-licensing: `UBX_MODULE_LICENSE_SPDX(MPL-2.0 BSD-3-Clause)`.

### Logging

Real-time safe, kernel-style levels. Set the node level with
`ubx-launch -l N`, override per block with an `int` config `loglevel`.
View the log with `ubx-log`.

```c
ubx_emerg(b, fmt, ...)	/* 0 system unusable */
ubx_alert(b, fmt, ...)	/* 1 immediate action required */
ubx_crit(b, fmt, ...)	/* 2 critical */
ubx_err(b, fmt, ...)	/* 3 error */
ubx_warn(b, fmt, ...)	/* 4 warning */
ubx_notice(b, fmt, ...)	/* 5 normal but significant */
ubx_info(b, fmt, ...)	/* 6 info */
ubx_debug(b, fmt, ...)	/* 7 debug: compiled out unless UBX_DEBUG is defined */
```

Outside of a block (e.g. in `module_init`):

```c
ubx_log(UBX_LOGLEVEL_ERROR, nd, __func__, "error %u", x);
```

Messages are truncated at `UBX_LOG_MSG_MAXLEN` (see
[build options](#build-options)).

### Guidelines

- use `long` for type related lengths and sizes: large enough, and
  errors can be returned negative (e.g. `cfg_getptr_uint32`).
- blocks with configurable data type and length use the canonical
  configs `type_name` and `data_len`.
- cache port pointers in `start` (or `init`): simpler, and saves a
  hash lookup per `step`. `ubx-genblock` does this.
- add `-fvisibility=hidden` to `CFLAGS` instead of making all
  functions `static`.
- configurable array size: see [saturation](std_blocks/saturation/saturation.c).
  Multiple types at compile time: [ramp](std_blocks/ramp/ramp.c), at
  runtime: [lfrb](std_blocks/lfrb/lfrb.c).

### C++ and Lua blocks

- C++: see [cppdemo](std_blocks/cppdemo/). Designated initializers for
  `ubx_proto_*` need g++ >= 8.
- Lua: see [luablock](std_blocks/luablock/README.md).

Composing systems
-----------------

Applications are described in *usc* files (microblx system
composition): which modules to import, which blocks to create, their
configs, connections and triggers:

```lua
return bd.system {
   imports = { "stdtypes", "ptrig", "lfrb", "mqueue", "ramp", "math_double" },

   blocks = {
      { name="ramp", type="ubx/ramp" },
      { name="sin", type="ubx/math_double" },
      { name="trigger", type="ubx/ptrig" },
   },

   configurations = {
      { name="ramp", config = { type="double", slope=0.05 } },
      { name="sin", config = { func="sin" } },
      { name="trigger", config = {
           period = { sec=0, usec=1000 },
           chain0 = { { b="#ramp" }, { b="#sin" } } } },
   },

   connections = {
      { src="ramp.out", tgt="sin.x" },                -- via ubx/lfrb
      { src="sin.y", type="ubx/mqueue" },             -- to ubx-mq
   },
}
```

```sh
ubx-launch -c app.usc
```

Beyond this, usc supports:

- **node configurations**: one config value shared by many blocks
- **subsystems**: compose systems from namespaced, reusable usc files
- **mixins**: merge platform specifics (e.g. RT triggers) at launch:
  `ubx-launch -c app.usc,ptrig_rt.usc`
- **model parameters**: `bd.param("PERIOD", 1000, "help")`, set with
  `ubx-launch -D PERIOD=500`
- **external blocks**: connect to blocks of an already running node

All features: [usc reference](docs/usc.md).

### Launching from C

Without Lua: [`examples/C/c-launch.c`](examples/C/).

Tools
-----

| tool           | purpose                                                     |
|----------------|-------------------------------------------------------------|
| `ubx-launch`   | launch usc files (`-h` for options)                         |
| `ubx-log`      | view the real-time log                                      |
| `ubx-mq`       | list, read and write `ubx/mqueue` iblocks                   |
| `ubx-modinfo`  | show module, block and type info: `ubx-modinfo show ramp`   |
| `ubx-genblock` | [generate a block](#generating-a-block)                     |
| `ubx-dbus`     | control a node via [lsdb-intf](std_blocks/lsdb-intf/README.md) |

Standard blocks
---------------

| Block                                                                 | Type      | Description                                                            |
|-----------------------------------------------------------------------|-----------|------------------------------------------------------------------------|
| [ubx/cconst, ubx/iconst](std_blocks/const/README.md)                  | c/i-block | constant value of any registered type                                  |
| [ubx/hexdump](std_blocks/hexdump/README.md)                           | i-block   | hex-dump written data to stdout (debug)                                |
| [ubx/lfrb](std_blocks/lfrb/README.md)                                 | i-block   | hard-RT lock-free ring buffer                                          |
| [ubx/lfds_cyclic](std_blocks/lfds_cyclic/README.md)                   | i-block   | hard-RT lock-free cyclic (overwriting) buffer                          |
| [ubx/vstore](std_blocks/vstore/README.md)                             | i-block   | single-slot value store for same-thread connections                    |
| [ubx/latch](std_blocks/latch/README.md)                               | i-block   | single-writer multi-reader latest-value store (seqlock)                |
| [mqueue](std_blocks/mqueue/README.md)                                 | i-block   | POSIX message queue inter-process communication                        |
| [ubx/math\_double, ubx/math\_float](std_blocks/math_double/README.md) | c-block   | element-wise math.h function (sin, sqrt, …) with optional scale/offset |
| [ubx/ewma](std_blocks/ewma/README.md)                                 | c-block   | exponentially weighted moving average filter (any numeric type)        |
| [ubx/movavg](std_blocks/movavg/README.md)                             | c-block   | sliding-window filter: mean, median, min or max (any numeric type)     |
| [ubx/mux, ubx/demux](std_blocks/mux/README.md)                        | c-block   | concatenate/partition array signals, incl. sub-vector slicing          |
| [ubx/stats](std_blocks/stats/README.md)                               | c-block   | running signal statistics: min, max, mean, stddev                      |
| [ubx/pid](std_blocks/pid/README.md)                                   | c-block   | discrete-time PID controller                                           |
| [ubx/ramp](std_blocks/ramp/README.md)                                 | c-block   | ramp signal generator (any numeric type)                               |
| [ubx/rand](std_blocks/rand/README.md)                                 | c-block   | pseudo-random number generator (any numeric type)                      |
| [ubx/saturation](std_blocks/saturation/README.md)                     | c-block   | element-wise signal clamp (any numeric type)                           |
| [ubx/threshold](std_blocks/threshold/README.md)                       | c-block   | threshold detector with crossing events and optional hysteresis        |
| [ubx/trig, ubx/ptrig](std_blocks/trig/README.md)                      | trigger   | passive and pthread-based triggers with timing stats                   |
| [ubx/gpio](std_blocks/gpio/README.md)                                 | c-block   | Linux GPIO via libgpiod v2                                             |
| [ubx/gps](std_blocks/gps/README.md)                                   | c-block   | GPS via gpsd shared memory interface                                   |
| [ubx/iio, ubx/iio_buf](std_blocks/iio/README.md)                      | c-block   | Linux IIO (ADC/DAC/IMU/sensor) via libiio                              |
| [luablock](std_blocks/luablock/README.md)                             | c-block   | generic LuaJIT block; implement hooks in Lua                           |
| [lsdb-intf](std_blocks/lsdb-intf/README.md)                           | c-block   | D-Bus interface to the ubx node                                        |
| [netsink](std_blocks/netsink/README.md)                               | lua block | UDP/ZeroMQ streaming sink (JSON/MessagePack), e.g. for PlotJuggler     |
| [webgraph](std_blocks/webgraph/README.md)                             | lua block | browser-based React Flow graph of the running node                     |
| [skelleton](std_blocks/skelleton/README.md)                           | template  | annotated starting point for new blocks                                |
| [cppdemo](std_blocks/cppdemo/README.md)                               | example   | minimal C++ block example                                              |

Tracing
-------

Beyond the built-in timing statistics of the trigger blocks (`tstats`,
min/max/avg per block or chain), microblx can be instrumented for
per-event tracing. The backend is selected at compile time and
defaults to off, in which case the trace macros compile to nothing:

```sh
cmake -DTRACING=SDT ..     # USDT probes (requires systemtap-sdt-dev)
cmake -DTRACING=MARKER ..  # ftrace trace_marker
cmake -DTRACING=OFF ..     # disabled (default)
```

The following events are emitted (see `libubx/ubx_trace.h`):

| event                    | args              | location                       |
|--------------------------|-------------------|--------------------------------|
| `chain_begin/chain_end`  | chain id          | around each trigger chain run  |
| `step_begin/step_end`    | block name        | around each c-block `step()`   |
| `overrun`                | missed, total     | ptrig missed a trigger deadline|
| `dl_overrun`             | total             | `SCHED_DEADLINE` budget overrun|

**SDT** compiles each probe to a single `nop` until a consumer
attaches to it, so it is safe to leave enabled in production
builds. The probes (provider `ubx`) can be consumed with `perf`,
`systemtap` or `bpftrace`, e.g. a per-block step latency histogram:

```sh
bpftrace -e '
usdt:/usr/local/lib/libubx.so.*:ubx:step_begin { @t[tid] = nsecs; }
usdt:/usr/local/lib/libubx.so.*:ubx:step_end /@t[tid]/ {
    @us[str(arg0)] = hist((nsecs - @t[tid]) / 1000); delete(@t[tid]); }'
```

**MARKER** writes the events into the kernel ftrace buffer (one
`write(2)` per event), where they interleave with kernel events on a
common timeline. This shows *why* a cycle overran (preemption, IRQs,
...), which no userspace-only profiling can:

```sh
trace-cmd record -e sched_switch -e irq ubx-launch -c app.usc
kernelshark trace.dat
```

Note that the node needs write access to
`/sys/kernel/tracing/trace_marker` (run `trace-cmd` as root, or mount
tracefs accordingly).

Lua API docs
------------

Generated with [ldoc](https://github.com/lunarmodules/LDoc), latest
on [GitLab Pages](https://kmarkus.gitlab.io/microblx). To build
locally (output in `<build>/lua-docs/`):

```sh
apt install lua-ldoc lua-discount luajit
cmake -DBUILD_LUA_DOCS=ON ..
make doc-lua
```

FAQ
---

### Running

**`blockXY.so: cannot open shared object file`**: the library is not
in the search path. Run `sudo ldconfig` after installing to a standard
location, or `export LD_LIBRARY_PATH=/usr/local/lib/`.

**Real-time priorities**: give luajit the capability and lock memory:

```sh
sudo setcap cap_sys_nice+ep $(which luajit)   # or a local luajit binary
ubx-launch --mlockall -c app.usc
```

From C, pass the node attribute `ND_MLOCK_ALL` to `ubx_node_init`.

**No core dumps with real-time priorities**: a safety mechanism of
`setcap` processes. Override with `ubx-launch --dumpable` or the node
attribute `ND_DUMPABLE`.

**luablock: "error object is not a string"**: the `strict` module is
loaded (directly or via `ubx.lua`) and the C code looks up an
undefined hook. Define all hooks or disable `strict` in the luablock.

**Script exits immediately**: run it with `luajit -i`, and with
`luajit`, not a standard Lua.

### Debugging

Core dump:

```sh
ulimit -c unlimited
gdb luajit core
(gdb) bt
```

Or run under gdb:

```sh
cd /usr/local/share/ubx/examples/usc/pid
gdb luajit --args luajit $(which ubx-launch) -c pid_test.usc,ptrig_nrt.usc
```

valgrind: define `UBX_CONFIG_VALGRIND` in `libubx/ubx.h`, so modules
are loaded with `RTLD_NODELETE` and traces in module code stay
meaningful:

```sh
valgrind --leak-check=full --track-origins=yes \
    luajit $(which ubx-launch) -t 3 -c examples/usc/threshold.usc
```

LuaJIT warnings like `Conditional jump or move depends on
uninitialised value` can be ignored (or build LuaJIT with
`-DLUAJIT_USE_VALGRIND`).

### Developing

**Block prototypes vs. instances**: prototypes are registered by
module init (`ubx_block_register`), instances cloned from them
(`ubx_block_create`). Use `blk_is_proto` and `blk_is_instance` to
tell them apart.

**meta-microblx: cross-compiling luajit fails with `asm/errno.h: No
such file or directory`**: install `gcc-multilib` on the build host.

Getting help
------------

Ask questions or report problems on the
[mailing list](https://groups.google.com/forum/#!forum/microblx)
(subscribe: `microblx+subscribe@googlegroups.com`). API changes are
tracked in the [ChangeLog](ChangeLog.md).

Related projects
----------------

- [meta-microblx yocto layer](https://github.com/kmarkus/meta-microblx)
- [microblx types for the Kinematics and Dynamics (KDL) library](https://github.com/kmarkus/microblx-kdl-types)
- [microblx connector blocks for ROS1](https://github.com/kmarkus/microblx-ros)
- [microblx motion control blocks and usc models](https://github.com/kmarkus/microblx-motion-control)
- hardware
  - [Kinova Kortex](https://github.com/rosym-project/robif2b)

Contributing
------------

Contributions are very welcome:

- follow the Linux kernel [coding style](https://www.kernel.org/doc/html/latest/process/coding-style.html).
- before submitting, run the tests (`./run_tests.sh` after
  installing) and static checking (`make cppcheck`, no output
  expected).
- submit patches via the mailing list or as a GitLab merge request.
- add `Signed-off-by: Random J Developer <random@developer.example.org>`
  to certify the
  [Developer's Certificate of Origin 1.1](https://developercertificate.org/).

License
-------

See COPYING. The microblx core is licensed under the weak copyleft
Mozilla Public License Version 2.0 (MPL-2.0). Standard blocks are
mostly licensed under the permissive BSD 3-Clause "New" or "Revised"
License or also under the MPLv2.

It boils down to the following. Use microblx as you wish in free and
proprietary applications. You can distribute binary function blocks as
modules. Only if you make changes to the core files (mostly the files
in libubx/) library), and distribute these, then you are required to
release these under the conditions of the MPL-2.0.

Acknowledgement
---------------

Microblx is considerably inspired by the OROCOS Real-Time
Toolkit. Other influences are the IEC standards covering function
block IEC-61131 and IEC-61499.

Microblx was supported by the European H2020 project RobMoSys via the
COCORF (Component Composition for Real-time Function blocks)
Integrated Technical Project.

This work was supported by the European FP7 projects RoboHow
(FP7-ICT-288533), BRICS (FP7- ICT-231940), Rosetta (FP7-ICT-230902),
Pick-n-Pack (FP7-NMP-311987) and euRobotics (FP7-ICT-248552).
