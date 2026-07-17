![microblx logo](/docs/user/_static/microblx-logo.svg)

microblx: hard realtime function blocks
=======================================

[![pipeline status](https://gitlab.com/kmarkus/microblx/badges/master/pipeline.svg)](https://gitlab.com/kmarkus/microblx/-/pipelines)
[![Documentation status](https://readthedocs.org/projects/microblx/badge/?version=latest)](http://microblx.readthedocs.io/?badge=latest)

Microblx is a lightweight and hard real-time safe function block
framework for use-cases such as *embedded control* or *signal
processing*. It provides generic and configurable code for typical
challenges such as *lock-free communication* between different
criticality domains, real-time safe logging or triggers with
configurable POSIX realtime properties.

Main features:

- **hard real-time safe**: no run-time allocations or non-deterministic system calls
- **simple model**: everything is a block
- **extensible**: easy to add a new type, trigger block or connection
- **composing applications**: applications are described using a simple textual language
- **standard blocks and tools** included:
  - *communication*: lock free buffers, POSIX message queues,
  - *computations*: PID controller, filters (moving average, EWMA),
    statistics, mux/demux, ramps, constants, random ...
  - *triggers*: periodic, passive incl. built in latency profiling
  - real-time safe logging
- **no HAL**: no arbitrary abstractions, just "configurable" POSIX
- **minimal**: tiny memory footprint, few dependencies, embedded-friendly

Quickstart
----------

Install core dependencies (Debian/Ubuntu):

```bash
apt install cmake pkg-config luajit libluajit-5.1-dev uthash-dev \
    libsystemd-dev libmxml-dev
```

Install Lua source dependencies:

```bash
git clone --depth=1 https://github.com/kmarkus/uutils.git
git clone --depth=1 https://github.com/corsix/ffi-reflect.git
cd uutils && sudo make install && cd ..
sudo install -d /usr/local/share/lua/5.1/
sudo cp ffi-reflect/reflect.lua /usr/local/share/lua/5.1/
```

Build and install microblx:

```bash
git clone https://gitlab.com/kmarkus/microblx.git
cd microblx && mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install && sudo ldconfig
```

Run an example:

```bash
ubx-launch -t 3 -c examples/usc/threshold.usc
```

For optional hardware blocks (`ubx/gpio`, `ubx/iio`, `ubx/gps`) install
`libgpiod-dev`, `libiio-dev libiio-utils`, or `libgps-dev gpsd`
respectively before building. See the full [install docs](https://microblx.readthedocs.io) for details.

Documentation
-------------

The documentation is [here](https://microblx.readthedocs.io) or can be
built locally (requires sphinx to be installed):

```bash
$ cd docs/
$ make html
Running Sphinx v1.8.5
...
```

### Lua API docs

The Lua binding API docs are generated with
[ldoc](https://github.com/lunarmodules/LDoc). Building them requires
`lua-ldoc`, `lua-discount` (for Markdown rendering), and `luajit`:

```bash
apt install lua-ldoc lua-discount luajit
```

Then enable and build via CMake:

```bash
cmake -DBUILD_LUA_DOCS=ON ..
make doc-lua
```

The generated HTML is written to `<build>/lua-docs/`. The latest
built docs are also available on [GitLab Pages](https://kmarkus.gitlab.io/microblx).

There is also a [ChangeLog](/ChangeLog.md) which summarizes API changes
and important features.

Getting help
------------

Please feel free to ask questions or report problems on the microblx
mailing list:

<https://groups.google.com/forum/#!forum/microblx>

It is possible to subscribe by email by sending a mail to
`microblx+subscribe@googlegroups.com`

Standard Blocks
---------------

| Block                                                                 | Type      | Description                                                            |
|-----------------------------------------------------------------------|-----------|------------------------------------------------------------------------|
| [ubx/cconst, ubx/iconst](std_blocks/const/README.md)                  | c/i-block | constant value of any registered type                                  |
| [ubx/hexdump](std_blocks/hexdump/README.md)                           | i-block   | hex-dump written data to stdout (debug)                                |
| [ubx/lfrb](std_blocks/lfrb/README.md)                                 | i-block   | hard-RT lock-free ring buffer                                          |
| [ubx/lfds_cyclic](std_blocks/lfds_cyclic/README.md)                   | i-block   | hard-RT lock-free cyclic (overwriting) buffer                          |
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

```bash
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

```bash
bpftrace -e '
usdt:/usr/local/lib/libubx.so.*:ubx:step_begin { @t[tid] = nsecs; }
usdt:/usr/local/lib/libubx.so.*:ubx:step_end /@t[tid]/ {
    @us[str(arg0)] = hist((nsecs - @t[tid]) / 1000); delete(@t[tid]); }'
```

**MARKER** writes the events into the kernel ftrace buffer (one
`write(2)` per event), where they interleave with kernel events on a
common timeline. This shows *why* a cycle overran (preemption, IRQs,
...), which no userspace-only profiling can:

```bash
trace-cmd record -e sched_switch -e irq ubx-launch -c app.usc
kernelshark trace.dat
```

Note that the node needs write access to
`/sys/kernel/tracing/trace_marker` (run `trace-cmd` as root, or mount
tracefs accordingly).

Related Projects
----------------

The following is a list of projects related to microblx:

- [meta-microblx yocto layer](https://github.com/kmarkus/meta-microblx)
- [microblx types for the Kinematics and Dynamics (KDL) library](https://github.com/kmarkus/microblx-kdl-types)
- [microblx connector blocks for ROS1](https://github.com/kmarkus/microblx-ros)
- [microblx motion control blocks and usc models](https://github.com/kmarkus/microblx-motion-control)
- hardware
  - [Kinova Kortex](https://github.com/rosym-project/robif2b)

Contributing
------------

Contributions are very welcome. Please check that the following
requirements are met:

- contributions must conform to the Linux kernel [coding
  style](https://www.kernel.org/doc/html/latest/process/coding-style.html).

- before submitting, please run the the tests (`./run_tests.sh` after
  installing) and static checking (`make cppcheck`, shouldn't output
  anything).

- patches can be submitted via the mailing list or as a gitlab merge
  request.

- please don't forget to add a line
  `Signed-off-by: Random J Developer <random@developer.example.org>`
  to certify conformance with the *Developer's Certificate of Origin
  1.1* below.

### Developer's Certificate of Origin 1.1

The sign-off certifies conformance with the
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
