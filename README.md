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
- [Composing systems](#composing-systems)
    - [Launching from C](#launching-from-c)
- [Tools](#tools)
- [Standard blocks](#standard-blocks)
- [Tracing](#tracing)
- [Lua API docs](#lua-api-docs)
- [FAQ](#faq)
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
  and out) and *hooks* called by the [life cycle](docs/blocks.md#hooks-and-life-cycle).
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

A minimal cblock:

```c
#include <stdlib.h>
#include <ubx.h>

char scale_meta[] = "{ doc='out = gain * in', realtime=true }";

ubx_proto_config_t scale_config[] = {
	{ .name = "gain", .type_name = "double", .min = 1, .max = 1 },
	{ 0 },
};

ubx_proto_port_t scale_ports[] = {
	{ .name = "in", .in_type_name = "double" },
	{ .name = "out", .out_type_name = "double" },
	{ 0 },
};

struct scale_info {
	const double *gain;
	ubx_port_t *p_in;
	ubx_port_t *p_out;
};

int scale_init(ubx_block_t *b)
{
	b->private_data = calloc(1, sizeof(struct scale_info));
	return b->private_data ? 0 : EOUTOFMEM;
}

int scale_start(ubx_block_t *b)
{
	struct scale_info *inf = b->private_data;

	if (cfg_getptr_double(b, "gain", &inf->gain) != 1)
		return -1;
	inf->p_in = ubx_port_get(b, "in");
	inf->p_out = ubx_port_get(b, "out");
	return 0;
}

void scale_step(ubx_block_t *b)
{
	struct scale_info *inf = b->private_data;
	double val;

	if (read_double(inf->p_in, &val) <= 0)
		return;		/* no new data */
	val *= *inf->gain;
	write_double(inf->p_out, &val);
}

void scale_cleanup(ubx_block_t *b)
{
	free(b->private_data);
}

ubx_proto_block_t scale_block = {
	.name = "oot/scale",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = scale_meta,
	.configs = scale_config,
	.ports = scale_ports,
	.init = scale_init,
	.start = scale_start,
	.step = scale_step,
	.cleanup = scale_cleanup,
};

int scale_mod_init(ubx_node_t *nd) { return ubx_block_register(nd, &scale_block); }
void scale_mod_cleanup(ubx_node_t *nd) { ubx_block_unregister(nd, "oot/scale"); }

UBX_MODULE_INIT(scale_mod_init)
UBX_MODULE_CLEANUP(scale_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
```

[`examples/oot-block`](examples/oot-block/) is this block plus a
custom config type and a CMake build against an installed microblx:

```sh
cd examples/oot-block && mkdir build && cd build
cmake .. && make && sudo make install
ubx-launch -c ../scale.usc
```

Configs, ports, the life cycle, types, logging, iblocks and triggers:
[block reference](docs/blocks.md).

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
| `ubx-launch`   | launch usc files, see [launching](docs/usc.md#launching)    |
| `ubx-ilaunch`  | like `ubx-launch`, then a Lua prompt with the node as `nd`  |
| `ubx-log`      | view the real-time log                                      |
| `ubx-mq`       | list, read and write `ubx/mqueue` iblocks                   |
| `ubx-modinfo`  | show module, block and type info: `ubx-modinfo show ramp`   |
| `ubx-tocarr`   | convert a type header to a `.hexarr` (used by builds)       |
| `ubx-dbus`     | control a node via [lsdb-intf](std_blocks/lsdb-intf/README.md) |
| `ubx-schedstat` | per-thread CPU time of a running node, worst case per period for sizing [`SCHED_DEADLINE`](std_blocks/trig/README.md#sched_deadline) `runtime_ns` |

```sh
ubx-log            # follow, incl. old messages
ubx-log -O         # only new messages
ubx-log -F         # dump the buffer and exit
ubx-log -N         # no colors
ubx-log -s -f LOCAL3        # also forward to syslog facility LOCAL3
ubx-log -d -s -O            # daemon, syslog only (needs libdaemon)

ubx-ilaunch -c app.usc
> ubx.block_tostate(nd:block_get("trig"), "inactive")

ubx-schedstat -n 60 1 1000   # 60 windows of 1 s, 1 kHz ptrig
```

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

**`blockXY.so: cannot open shared object file`**: run `sudo ldconfig`
after installing, or `export LD_LIBRARY_PATH=/usr/local/lib/`.

**Real-time priorities**: run with `CAP_SYS_NICE` as an ambient
capability and lock memory:

```sh
sudo -E capsh --keep=1 --uid="$(id -u)" \
     --inh=cap_sys_nice --caps=cap_sys_nice+eip --addamb=cap_sys_nice -- \
     -c 'exec ubx-launch --mlockall -c app.usc'
```

Don't `setcap` the luajit binary: file capabilities make sd-bus ignore
the session bus environment, which breaks `--dbus`. Example:
[`run-pid.sh`](examples/usc/pid/run-pid.sh). From C, pass the node
attribute `ND_MLOCK_ALL` to `ubx_node_init`. If core dumps are
missing, pass `--dumpable` (`ND_DUMPABLE`).

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
