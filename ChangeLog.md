ChangeLog
=========

This file tracks user visible API changes

## 1.0.0

- `ptrig`: new `sleep_mode` 2 (hybrid): sleeps for
  `period - busy_slack_ns` and busy-waits the remaining
  `busy_slack_ns`, which absorbs the OS wakeup latency and yields
  busy-wait accuracy at a `busy_slack_ns / period` duty cycle. Tuned
  via the new `busy_slack_ns` config (default 50000). The value must
  exceed the platform's worst-case wakeup latency or the mode degrades
  to `sleep_mode=0`.
- `ptrig`: new `timerslack_ns` config, applied to the trigger thread
  via `prctl(PR_SET_TIMERSLACK)`; 0 (default) leaves the inherited
  value unchanged. Linux defaults non-realtime threads to 50us of
  timer slack, which shows up directly as trigger lateness under
  `SCHED_OTHER`; realtime policies ignore it.
- `libubx`: new `ubx_nanosleep_hybrid(const struct ubx_timespec *dur,
  uint64_t slack_ns)`, the sleep-then-spin wait backing ptrig's hybrid
  sleep mode.

- `rtlog`: reimplemented on top of `liblfb`, the lock-free broadcast
  buffer. The shm writer spinlock is replaced by a process-shared,
  robust, priority-inheritance mutex (held in lfb's user area) and the
  write offset is a C11 atomic with release/acquire publication. This
  bounds priority inversion among writers of different priorities,
  recovers when a writer dies while holding the lock (`EOWNERDEAD`)
  instead of deadlocking all loggers, and fixes reader-side memory
  ordering on weakly-ordered architectures. The log shm layout
  changed: mixed-version processes must not share a log shm (a foreign
  or stale segment is rejected/reinitialized).
- `struct ubx_log_msg`: **ABI change** — `ts` is now an `int64_t`
  nanosecond timestamp (was `struct ubx_timespec`) and `level` an
  `int32_t`, giving a fixed-width, ABI-stable layout identical on 32-
  and 64-bit targets. The struct is now 192 bytes (three cache lines),
  so log frames never straddle cache lines. `UBX_LOG_MSG_MAXLEN`
  shrank from 127 to 115; longer messages truncate 12 bytes earlier.
  It is now a CMake cache variable (`-DUBX_LOG_MSG_MAXLEN=`, default
  115): `ubx_types.h` is generated from `ubx_types.h.in` so the value
  reaches both the C build and the luajit FFI. rtlog asserts the frame
  stays a cache-line multiple, so keep values of the form 64*k - 77
  (51, 115, 179, ...).
- `librtlog_client`: the pkg-config `Cflags` now also add
  `-I${includedir}/ubx` (the client header includes `lfb.h`).
- `saturation`: **API change** — the compile-time typed
  `ubx/saturation_*` blocks were replaced by a single runtime-typed
  `ubx/saturation` block: element type and length are set via the
  `type` and `data_len` configs; `lower_limits`/`upper_limits` are
  `double` arrays of length 1 (broadcast) or `data_len`.
- `ramp`: added the generic `ubx/ramp` block: element type selectable
  at runtime via `type` (all standard numeric types), with per-type
  native accumulation kernels (exact for 64-bit integers beyond the
  2^53 double mantissa). `slope` is mandatory, `start` defaults to 0;
  both broadcast scalars to `data_len`. The legacy `ubx/ramp_*`
  blocks are retained for backwards compatibility.
- `rand`: added the generic `ubx/rand` block (runtime `type` +
  `data_len`). Uses per-instance PRNG state (`erand48`/`jrand48`), so
  multiple instances are thread-safe and independent; seeding via the
  `seed` config is `srand48`-compatible. The legacy `ubx/rand_*`
  blocks are retained for backwards compatibility.
- `threshold`: added an optional `hysteresis` config (Schmitt
  trigger): the state switches to 1 only above `threshold +
  hysteresis/2` and back to 0 only below `threshold - hysteresis/2`,
  suppressing event chatter on noisy signals.
- `movavg`: added a `mode` config selecting the sliding-window
  aggregate: `mean` (default), `median`, `min` or `max`. The median
  is RT-safe (insertion sort on a preallocated scratch buffer).
- `netsink`: the zmq transport falls back to loading the versioned
  `libzmq.so.5` SONAME when the unversioned `libzmq.so` (a
  dev-package symlink not shipped on production images) is absent.
- `libubx`: `ubx_data_free` is now NULL-safe (like `free(3)`),
  simplifying block error paths.
- `examples`: updated to use the generic `ubx/ramp` and `ubx/rand`
  blocks.
- `libubx`: added optional `preinit`/`preexit` life-cycle hooks
  (`ubx_proto_block_t` / `ubx_block_t`). `preinit` runs while the block
  is in state `preinit` (before the regular config is applied and before
  `init`), letting a block extend its interface (add/resize ports,
  create configs) based on its static configuration. `preexit` is its
  teardown counterpart, run on block removal. New API:
  `ubx_block_preinit()` and `ubx_block_preexit()`; `ubx_block_init()`
  runs `preinit` automatically. The change is purely additive and
  backwards compatible (blocks recompile against the new headers). The
  deployment (`blockdiagram`) now applies configuration in an extra pass
  between `preinit` and `init` so that configs created by `preinit` can
  be set before `init` reads them. The hooks are also available to
  `luablock` (Lua `preinit`/`preexit` functions) and via the Lua API
  (`block_preinit`/`block_preexit`).
- `ubx_time`: **API change** - replaced `ubx_nanosleep(int flags, struct
  ubx_timespec *ts)` with two new functions that take relative durations:
  `ubx_nanosleep(const struct ubx_timespec *dur)` which yields the CPU
  using `clock_nanosleep`, and `ubx_nanowait(const struct ubx_timespec
  *dur)` which busy-waits using `ubx_gettime`. Both functions use
  `ubx_gettime` for time retrieval, benefiting from hardware timestamping
  (TSC/CNTVCT) when available.
- `libubx`: `ubx_module_load` returns `EENTEXISTS` when a module is
  already loaded. Lua `load_module` has been changed to log and ignore
  opposed to error in this case.
- `blockdiagram`: added support for directly loading luablocks via the
  `luablock:LUABLOCKNAME` syntax.
- `lsdb-intf`: added D-Bus interface block for remote controlling a
  ubx system via D-Bus or the command-line tool `ubx-dbus`.
- `ubx_time`: added support for hw timestamps on arm64 via CNTVCT
- drop the ffi `lua/reflect.lua` library from the repo. It must be
  installed from `https://github.com/corsix/ffi-reflect`.
- add `liblfq`: a minimal and portable lock-free queue based on
  Vyukovs algorithm. lfds currently doesn't run on aarch64|arm64 and
  this implementation provides a simple yet efficient replacement.
- add `ubx/lfrb`: hard-RT lock-free ring buffer iblock based on
  `liblfq`. On platforms where `lfds_cyclic` is unavailable (aarch64)
  this is the default connection iblock.
- `lfds_cyclic`: temporarily disabled while `liblfds` is unavailable
  upstream; use `lfrb` as a drop-in replacement.
- `ubx.lua`: auto-detects whether `lfrb` or `lfds_cyclic` is available
  and selects the available one as the default connection iblock.
- switch to `cmake`
- replace `lua-filesystem` with ffi implementation

New blocks:

- `ubx/ewma`: exponentially weighted moving average filter (`y +=
  alpha * (x - y)`) for any numeric type, configured at runtime via
  `type`/`data_len`. The mandatory `alpha` ∈ (0, 1] sets the
  smoothing (rule of thumb: `alpha = 2/(N+1)` approximates a window
  of N samples); the first sample seeds the filter state. See
  `std_blocks/ewma/README.md`.

- `ubx/mux`, `ubx/demux`: composition glue for array signals of any
  registered type: `mux` concatenates `nin` input ports into one
  vector output, `demux` partitions a vector input onto `nout` output
  ports. The optional `in_len`/`out_len` configs assign per-port
  sub-vector lengths, which makes `demux` double as a slice/splice
  block — connect only the outputs needed and leave the rest
  unconnected. See `std_blocks/mux/README.md` and the runnable
  `std_blocks/mux/slice.usc` demo.

- `netsink`: network streaming sink for live plotting and telemetry,
  primarily for [PlotJuggler](https://plotjuggler.io). Implemented as a
  plain `ubx/luablock` (no core extension). Configurable `transport`
  (`udp` datagrams or a ZeroMQ `PUB` socket, via the LuaJIT FFI — no
  `luasocket`/`lzmq` dependency) and `format` (`json`, or MessagePack
  via `lua-MessagePack`). All settings -- the port set and the network
  configuration (transport/format/host/port/uri) -- are declared as
  `lua_str` globals. Timestamps use a connected `ts` input port when
  present, otherwise `ubx_gettime()`. Supports RT/NRT decoupling: run the
  sink on the luablock self-trigger (`thread=1`, `period`) at a lower
  rate than the producers and size connection buffers accordingly.
  Per-port pending/grace slots align frames across non-atomically filled
  buffers. See `std_blocks/netsink/README.md`.

- `ubx/gpio`: Linux GPIO block via `libgpiod` v2
- `ubx/iio`, `ubx/iio_buf`: Linux IIO (ADC/DAC/IMU/sensors) via `libiio`
- `ubx/gps`: GPS block via `gpsd` shared memory interface
- `ubx/math_float`: element-wise `math.h` functions for the `float` type
  (complement to the existing `ubx/math_double`)
- `webgraph`: browser-based React Flow graph of the running node;
  `ubx-launch` gained a `-webgraph [PORT]` option (default port 8888)

Removals:

- `webif` block **removed** — superseded by `webgraph` (visual graph) and
  `lsdb-intf` (programmatic control). The `ubx-launch -webif` option is
  gone; use `-webgraph` instead.
- `ubx.lua`: **removed** `M.node_todot` (Graphviz DOT output), which was
  only used internally by `webif`.
- `ubx.lua`: **removed** `M.color` flag and all color-related helpers;
  `ansicolors` is no longer a dependency of `ubx.lua`. Colorized output
  is now handled per-tool where needed.

Other changes:

- `ubx.lua`: added OO predicate methods on `ubx_block_t`: `is_cblock`,
  `is_iblock`, `is_proto`, `is_instance`, `is_active`, `is_trigger`,
  `is_realtime` and composite variants. Added direction predicates on
  `ubx_port_t`: `is_inport`, `is_outport`, `is_inoutport`.
- `luablock`: added built-in self-triggering support via the `trigger`
  (set to `1` to enable) and `period` (float, seconds) configs, removing
  the need for a separate `ptrig` in non-RT use-cases.
- `luablock`: the self-trigger thread is named after the block using
  `pthread_setname_np`; availability is detected at build time via CMake
  `check_symbol_exists` and applied as a `PRIVATE` compile definition.
- `ptrig`: added `SCHED_DEADLINE` support (Linux ≥ 3.14). Set
  `sched_policy` to `"SCHED_DEADLINE"` and provide `sched_deadline =
  { runtime_ns, deadline_ns, period_ns }` — `deadline_ns` and
  `period_ns` default to the `period` config when zero. The kernel
  enforces `runtime_ns ≤ deadline_ns ≤ period_ns`; ptrig validates
  this at init. Budget overruns are caught via `SIGXCPU` (Linux ≥
  4.16), logged, and counted on the new `deadline_throt_cnt` output
  port. `sched_yield(2)` replaces the normal sleep after each
  trigger. A `sched_deadline` in-port allows updating the parameters
  at runtime. See `std_blocks/trig/README.md` for details.
  New example: `examples/usc/pid/ptrig_deadline.usc`.
- `ptrig`: `sleep_mode != 0` with `SCHED_DEADLINE` is now a hard init
  error; `sched_setattr` failure properly stops the block.
- `cconst`/`iconst`: added an `in` port (same type and length as the
  held value) to update the initially configured value at runtime. The
  c-block reads `in` once at the start of `step()`; the i-block reads
  it once at the start of `read()` before copying the value out.
- `ptrig`: `SIGXCPU` overrun delivery restricted to the DEADLINE
  thread; fixes spurious node teardown via `ubx_wait_sigint`.
- `ubx_wait_sigint`: retry `sigtimedwait` on `EINTR`.
- `examples/usc/pid/run-pid.sh`: unified nrt/rt/deadline launcher.
- `ptrig`: new `sleep_mode` config — `0` (default): OS sleep via
  `clock_nanosleep`; `1`: busy-wait using `ubx_nanowait`.
- `lsdb-intf`: added plugin support; Lua modules loaded at init time can
  extend the D-Bus interface with new methods. `ubx.info`, `ubx.warn`,
  `ubx.err` etc. are available in the plugin context. `ClearNode` gained
  a `keeplist` parameter to exclude specific blocks from removal.
- `ubx-dbus`: added colorized pretty-print output for node info (`-i`)
  and per-block info (`-i BLOCK`).
- `ubx-modinfo`: added standalone color output with the `-c` flag.
- `blockdiagram`: added `extern_blocks` support — blocks listed there are
  expected to exist in the node already and will not be created or
  destroyed by the blockdiagram launch/cleanup logic.
- `ubx-log`: added syslog forwarding (`-s`) — messages are tee'd to
  syslog in addition to stdout; facility selectable via `-f LOCAL<n>`
  (LOCAL0–LOCAL7, default LOCAL0). Added daemon mode (`-d`, requires
  `-s`; needs `libdaemon` at build time) — properly daemonizes via
  `libdaemon`, writes a PID file at `/run/ubx-log.pid`, and prevents
  duplicate instances.

## 0.9.2

bugfix release:

- blockdiagram: don't reject connections among iblocks
- ubx.lua: handle different libubx naming schemes

## 0.9.1

- `trig`, `ptrig`, `trig_utils`: add `every` member to configure
  reduced frequency triggering. If set, the respective block will be
  only triggered every `every` time with respect to the base
  frequency.

- `blockdiagram`, hierarchical composition: add support for adding
  subsystems *without a namespace*, i.e. effectively merging them. If
  a subsystem is added to the `subsystems` table *without* a name,
  then it will me merged similar to when multiple models are provided
  on the cmdline. However, in the latter case, the merged models
  override existing entries, whereas when specified in `subsystems`,
  the parent subsystem takes precedence. This feature is mainly useful
  to avoid deep hierarchies.  In addition this cleans up the merge
  function and adds a verbose `-v` option to `ubx-launch`, for
  printing early messages (mainly merge actions for now).

- `ubx-mq`
    - add support for writing to mqueues. Example:
      `ubx-mq write mymq  "{0,0.01,0,0.1,0}" -r 0.1`
    - show types in mq listing (this requires latest `uutils` v1.1.0.

- rtlog:
    - move spinlock into shm and initialize with
      `PTHREAD_PROCESS_SHARED` cmdline tools such as `ubx-mq` create
      processes and nodes that log to the same shm. Hence we set the
      default to shared. Users that really need to squeeze performance
      can explicitely set this back to `PTHREAD_PROCESS_PRIVATE`.

    - don't delete the log shm upon cleanup and don't truncate it
      during node init if the size is correct. This avoids unnecessary
      reopening (incl. seeks backs) especially in multi-node setups
      and facilitates debugging by preserving the log buffer.

- `ubx.lua`: add generic `connect` function. This replaces
  `conn_lfds_cyclic` and `conn_uni` and allows the following
  connection types:
  - cblock to cblock
  - cblock to existing iblocks (and vice versa)
  - cblock to non-existing iblock (and vice versa). In this case the
	iblock block will be created.

  See the user docs for more information.

- `ubx_ports_connect`: check and enforce that types and data length
  match.

- `lfds_cyclic`: make `buffer_len` config optional and let it default
  to 1 if unset.

- removed the deprecated `file_logger` block. This was messy and hard to
  maintain. The preferred approach to log is now by using `ubx/mqueue`
  blocks and `ubx-mq(1)`.

- `std_blocks`: cleanup block naming. all blocks under `std_blocks/`
  are now called `ubx/BLOCKNAME`.

## 0.9.0

- typemacros: added `def_cfg_set_fun` to define type safe
  configuration setters akin akin to `def_cfg_getptr_fun`. See
  `examples/platform/platform_launch/main.c` for a usage example.
  
- std_blocks: added saturation block. This multi-length block will
  limit the given signal to the configured minima and maxima.

- trig, ptrig: added multi-chain (i.e. formerly called `trig_blocks`
  lists). The desired number of chains can now be configured using
  `num_chains` and it can be switched among these via the
  `active_chain` port. To make this feasible, a number of cleanups and
  renames was done in trig_utils:

   - the previous `trig_blocks` config was renamed to
	 `chain0`. Additional chains are called `chain1`, `chain2` etc.
   - `struct trig_info` was renamed to `struct ubx_chain`
   - `trig_info_*` API was renamed to `trig_chain_*`
   - `stuct ubx_trig_spec` was renamed to `struct ubx_triggee`
   - `tstat.block_name` renamed to `tstat.id`

- add function based predicates `blk_is_*`, `port_is_*` and `cfg_is_*`
  to replace the old `IS_` macros .

- `lfds_cyclic`: added `allow_partial` config to allow transporting
  messags that are < `data_len`. If unset, the new default is to
  reject these. This also extends the lua port_clone_conn function
  with an optional `allow_partial` parameter.

- `ubx_node_info_t` renamed to `ubx_node_t` for
  consistency. `ubx_node_t` variables renamed from `ni` to `nd`.

- `ubx-genblock` updated to generate state of the art blocks

- `ubx_ports_connect_uni`/`ubx_ports_disconnect_uni`/
  - renamed: dropped `_uni` suffix
  - made iblock parameter `const`

- `ubx_block_`: store `ports` and `configs` in a `utlist.h` double
  linked list instead of a dynamic array
  ([background](docs/dev/002-stable-port-ptrs.md)).

- registration of static blocks has changed. Use the types
  `ubx_proto_block_t`, `ubx_proto_port_t` and `ubx_proto_config_t` to
  define blocks definition and `ubx_block_register(ubx_node_t*,
  ubx_block_proto_t* bt)` to register them. Note that this **only**
  changed for the static definition/registration of blocks. At runtime
  (i.e. in hooks etc) the non-`_proto_` versions are used as before
  ([background](docs/dev/002-stable-port-ptrs.md)).

- `ubx_config_add2` adds a config incl `min`/`max` constraints and
  attrs.

- The attributes `CONFIG_ATTR_CLONED` and `PORT_ATTR_CLONED` are set
  for ports/configs that were created as a consequence of creating a
  block by cloning. Put differently, these attributes are unset for
  configs and ports that were added dynamically.

- `ubx_port_add` `ubx_inport_add`, `ubx_outport_add`: the parameter
  `state` has been dropped (as has the member in `ubx_port_t`. Instead
  `attrs` has been added as parameter #4. Check `ubx_proto.h` for the
  new prototypes.

- `ubx_types.h`: converted object names (node, block, port, config)
  from dynamically allocated strings to fixed strings where
  possible.

- `ubx_types.h`:
   - removed port `state`, which was never used. The functions
	 `ubx_port_add` (lua `ubx.port_add`) consequently drop the last
	 parameter.

   - removed port direction attributes `PORT_DIR_OUT` and
	 `PORT_DIR_IN`. Instead this is determined from a non-NULL `in_`
	 or `out_type` ptr. The macros `IS_OUTPORT`, `IS_INPORT` and
	 `IS_INOUTPORT` do this in a portable way.

   - removed port statistics (were never used)

- renamed constants for consistency:
  - `BLOCK_NAME_MAXLEN` -> `UBX_BLOCK_NAME_MAXLEN`
  - `PORT_NAME_MAXLEN` -> `UBX_PORT_NAME_MAXLEN`
  - `TYPE_NAME_MAXLEN` -> `UBX_TYPE_NAME_MAXLEN`
  - `LOG_MSG_MAXLEN` -> `UBX_LOG_MSG_MAXLEN`
  - `TYPE_HASH_LEN` -> `UBX_TYPE_HASH_LEN`
  - `TYPE_HASHSTR_LEN` -> `UBX_TYPE_HASHSTR_LEN`

## v0.8.1

- doc improvements:
  - added `examples/usc/threshold.usc` gentle example
  - `docs/user/` introduction improved

- added `math_double` block which supports a large number of `math.h`
  functions.

- `std_blocks/examples` cleanup
   - `simple_fifo` and `random` moved under `examples/`
   - added `skelleton.c` as a minimal but complete empty block
   - added `threshold` example block for use in simple usc intro

- `trig`, `ptrig`:
   - indepent of `tstats_output_rate`, all stats are written to the
	 port `tstats` (if it exists) in `stop()`.
   - config `autostop_steps` added to stop ptrig after triggering this
	 number of times.

- `port_clone_conn`: add `loglevel_overruns` argument.

- `const` block refactored into a generic block that is configured
  with the ubx `type_name` and `data_len`, and then with the actual
  value of the type. Both a cblock `cconst` and an iblock `iconst`
  variant are available.

- blockdiagram: small extension to support dynamic configurations. If
  a config does not exist in state `preinit`, configuration will be
  retried in `inactive`.

- core: add `ubx_node_clear`. This function stops, cleans up and
  removes all non-prototype blocks.

- `ptrig`: add CPU `affinity` config (optional). This takes a list of
  the cores to run on. Default is no affinity.

- `lfds_cyclic`: add `loglevel_overruns` config. This allows
  configuring the loglevel of overrun conditions, or silencing them
  completly by setting the value `-1`.

## v0.8.0

- triggers: if `num_steps` is 0, this will be interpreted as the
  default (1 step). To disable triggering of a block, set to -1.

- Accessors for all stdtypes are defined using `def_type_accessors`,
  hence blocks can use these instead of defining their own.

- core: cleanup of port read/write and config accessors. The macros

  - `def_write_fun` and `def_read_fun`
  - `def_write_arr_fun` `def_read_arr_fun`
  - `def_write_dynarr_fun` and `def_read_dynarr_fun`

  have been deprecated in favor of the following new ones (the macro
  parameters are the same, so simple renaming will work in most cases):

  - `def_port_readers(FUNCNAME, TYPENAME)`
	`def_port_writers(FUNCNAME, TYPENAME)`

  will each define both single element function as well as the array
  version, e.g. for the writer side:

	`long FUNCNAME(const ubx_port_t* p, TYPENAME* val)`
	`long FUNCNAME_array(const ubx_port_t* p, TYPENAME* val, const int len)`

  For convenience, the the macros `def_port_accessors(SUFFIX,
  TYPENAME)` and `def_type_accessors(SUFFIX, TYPENAME)` can be used to
  define both port readers and writers _or_ port readers, writers and
  config accessors respectively (Note: these take a function name
  suffix instead of the whole name).

  Internally, the implementation is both safer and significantly
  faster due to avoiding a hash table lookup.

- lua: `conn_uni`: iblock name generation scheme changed from using a
  timestamp to an incrementing counter. This is to facilitate testing,
  that can rely on reproducable names.

- tools: `ubx_launch`, `ubx_ilaunch`, `ubx_genblock`, `ubx_log` and
  `ubx_tocarr` are renamed to `ubx-launch`, `ubx-ilaunch`,
  `ubx-genblock`, `ubx-log` and `ubx_tocarr` respectively.

- convert `CONFIG_DUMPABLE` and `CONFIG_MLOCK_ALL` to node
  attributes. These can now be set as `ubx-launch` parameters (or at
  C-API level by setting the respective bits in the `attrs`
  parameter).

- doc: generate block reStructuredText docs using `ubx-modinfo`

- core: added `ubx_module_get` function and extended Lua `load_module`
  to return the module id (i.e. path from which it was loaded). The
  latter can be passed to the former function (e.g. for looking up an
  spdx license id).

- core: added functions for `ubx_type_t` lookup via hash or hashstr.

- added **PID controller** block and **extended ramp block** with a
  `data_len` parameter to allow vector ramp output. Added helper
  functions for reading/writing arrays (`def_write_dynarr_fun` and
  `def_read_dynarr_fun`) where the size is determined at
  run-time. Added helpers for resizing port data lenght
  (`ubx_inport_resize` and `ubx_outport_resize`).

- `ubx_launch`: `-c` options now supports multiple arguments and will
  *merge all given models* into the first before launching. Use-case
  is for late addition of policy (e.g. `ptrig` blocks etc.) to
  otherwise reusable compositions.

- **blockdiagram**: *drop* the `start` directive that was used to pass
  the last blocks to be started (i.e. ptrig) to the launch logic. This
  is not necessary anymore, as the launch logic will ensure to startup
  active blocks (see `BLOCK_ATTR_ACTIVE`) after all others.

- **blockdiagram**: add *composition support* to permit hierarchical
  composition of usc systems. This change introduces the `subsystems`
  keyword and is backwards compatible with existing compositions.

- `ubx-launch`: `-s` option added for printing usc launch messages to
  stderr (in additon to the rtlog). Useful for debugging when long
  messages may get truncated in the rtlog.

- **triggering**: `trig`, `ptrig`, `trig_utils`

  - `tstat_utils`: rename to `trig_utils`. Created generic trigger
	 implementation in `trig_utils` for use by `trig` and `ptrig` as
	 well as custom user `triggers`.

  - `ptrig` and `trig` converted to use generic trigger handling from
	`trig_utils`

  - `tstats_enabled` renamed to `tstats_mode`. Values are
	- 0: off
	- 1: total tstats only
	- 2: total and per block tstats

  - `ptrig` and `trig`: new config `tstats_output_rate` for
	configuring how often statistics are written to the tstats port.

  - config `tstats_print_on_stop` dropped. New behavior is: if
	`tstats_mode` is non-zero, then stats will be logged on stop.

- `stattypes`: merge into `stdtypes` as this only contains the
  ubx_tstat type. Hence, there is no need to import `stattypes`
  anymore.

## v0.7.1

- core: add environment variable `UBX_PATH` that can be used to
  specify alternative installation paths. If defined, ubx will search
  the colon separated list of directories for ubx headers and modules.

- core: ffi: types: strip preprocessor directives from types that are
  fed to the ffi. This is to allow include guards in these header
  files.

- config: add `min`/`max` specifiers for introducing constraints such
  as optional or mandatory configs.

- struct ubx_block: added attributes variable (`attrs`) and defined
  first block attributes (`BLOCK_ATTR_TRIGGER` and
  `BLOCK_ATTR_ACTIVE`). These will be used by tools such as
  ubx_launch, so blocks should be updated to define these
  appropriately.

- blockdiagram.load: change to return `system` or fail and exit

- rtlog: enable gcc format checking. This checks the provided
  arguments match the given format string.

- ubx_tocarr: cmdline args added to allow overriding output file file

## v0.7.0

- ubxcore: `ubx_node_cleanup` changed to remove blocks and unload
  modules, but to leaving the node intact. For destroying a node,
  `ubx_node_rm` was introduced.

- ubx.lua: `config_get` returns `nil` if a config doesn't exist
  instead of throwing and error

- added `ubx-modinfo` tool for offline module introspection

- lfds_cyclic: increased loglevel for buffer overflow logging to
  NOTICE.

- introduced real-time safe logging and switched core and std_blocks
  to it

- `data_size`: changed return value to `long` (signed) to be able to
  signal errors via negative return value.

## v0.6.2

- renamed TSC configure option `tsc-timers` to `timesrc-tsc`

- added hooks for extra checking `-check` and `-werror` and two checks
  for unconnected in- and outports.

- removed generated `ubx_proto.h` from git

- cleanup: headers moved to `include/ubx/`. pkg-config file adapted,
  so this change should be transparent.

- trig, tstat: timing statistics support generalized for use in
  non-core blocks.

- support for using TSC timers (x86) as a low-overhead time source for
  timing statistics.

- `ubx.lua` use unversioned `libubx.so`

- bugfix in `ubx_port_add` (see 28e6d8acc42)


## v0.6.1

- ubx_launch, blockdiagram: exit and complain loudly if launching a
  blockdiagram fails

## v0.6.0

- updated all blocks to new config handling

- switch len and ptr* `ubx_config_get_data_ptr` to be streamlined with
  the new `cfg_getptr_*` utilities and to allow reporting errors. The
  new prototype is:

```C
 `long ubx_config_get_data_ptr(ubx_block_t *b, const char *name,
 void **ptr)
 ```
  len < 0 signifies and error, 0 that the config is unset and > 0
  means configured and array lenght of `len`.

- configurations are not initalized to size 1 by default. This was
  useful back in the day when most configurations were of len 1, but
  this is not true anymore and it makes in cumbersome to distinguish
  between unconfigured and configurations with value 0. This is mainly
  visible through `ubx_config_get_data_ptr`, which may now return
  `NULL` and len=0. Blocks must handle this case.

- `cfg_getptr_[char|int|...]` convenience functions added. These
  functions allow retrieving configuration in a somewhat more typesafe
  manner. Example:

  ```C
  long len;
  uint32_t *val;

  len = cfg_getptr_uint32(b, "myconf", &val);

  if(len == 0)
	  /* myconf unconfigured */
  else if(len > 1)
	  /* myconfig configured, use val */
  else
	  /* len < 0, error */
  ```

- removed unused and deprecated `lfds_cyclic_raw` block.

## v0.5.1

- blockdiagram, ubx_launch: moved node config from `configurations` to
  a separate top level section `node_configurations`. This avoids that
  `configurations` becomes a hybrid dictionary/array which causes
  problems when importing from json.

- pkg-config: `ubx_moddir` renamed to `UBX_MODDIR` for consistency.

## v0.5.0

- support for node configuration in C and via the `ubx_launch`
  DSL. Using the node (global) configuration mechanism multiple block
  configurations can be setup to refer to the same configuration
  value. This way, only one "screw" needs to be turned to change the
  configuration for all blocks, and the required memory is only
  allocated once.

  Note that it is transparent to the blocks whether they are
  configured individually or via a node config.

  For an example, take a look at
  [node_config_demo.usc](examples/systemmodels/node_config_demo.usc)

- the static initialization of block configuration array length has
  changed during the refactoring for node config support:

```C
/* old way */
ubx_config_t blk_conf[] = {
	{ .name="foo", .type_name="char", .value = { .len=32 }
	{ NULL }
};
```

```C
/* new way */
ubx_config_t blk_conf[] = {
	{ .name="foo", .type_name="char", .data_len=32 }
	{ NULL }
};
```

  if you didn't use this (probably the case for most blocks), there's
  nothing to change.

## v0.4.0

- `ubx_launch`: shutdown option `-b` added to shutdown when a
  (trigger) block is stopped.

- `ubx_data_free`: removed unused ni argument

- enabled full warnings `-Wall -Wextra -Werror` and fixed all
  occurences.

- added `ubx_outport_add` and `ubx_inport_add` convenience functions.

- thorough cleanup of `ptrig`:
  - removed ifdef'ed timing statistics
  - removed MAX_BLOCKS define for number of timing blocks
  - `tstats_print_on_stop` config to print statistics


- `ubx_types.h`: reduce `BLOCK_NAME_MAXLEN` to 30

- more tests added: ptrig

- various fixes

## v0.3.4

- `ramp` block generalized to support all basic types

- various fixes

## v0.3.4

- simple ramp block added

- `blockmodel`: pulldown support added.

- promoted `get_num_configs` and `get_num_ports` to public IF.

- `ubx_ts_to_ns` added to convert `ubx_timespec` to uint64_t ns.

- `ubx_launch`: -t added to run for a specific amount of time.

## v0.3.3

- `ubx_launch` start directive
  e5a716c4c096020ec0767a7eb87c3952977919a2

- `ubx_launch` JSON support 4cc5fc83ae966153289c4dc9733d77737f7342e1

- various smaller cleanups

## < v0.3.2
- support for type registration in C++ added
  0736a5172e7fb2a93dc7c9f42c525c0e5626ae0e

- introduce `ubx_ilaunch` for interactive mode
  1725f6172d92730044928ffaf984ec8b57be8bcb. Renamed non interactive
  version to `ubx_launch`.

- `file2carr.lua` renamed to `ubx_tocarr`
  a3eee190b93423a49a678d20b239bc1dd30a3444

- install core modules into `$(prefix)/lib/microblx/<major.minor>/`
  51e1bbb6c79dc3aa9081802334c54f2c5bab1270

- [9ff5615f9d] removed std Lua modules provided by separate package
  `uutils` (see README.md).

- [469e930b8a] Clang dependency removed: compiling with gcc works when
   using initialization as demonstrated in `std_blocks/cppdemo`.

- [4213621d55] merged support for autotools build

- [4e3a0b9a72] OO support for node, block, port, config, data and
  type. Removed old and unused Lua functions.

- [65cd7088c6] interaction blocks are not strongly typed, i.e. instead
  if data element size and number of buffer elements, the type name,
  array len and number of buffer elements are given. This will help
  ensure type safety for inter-process iblocks.

- [575d26d8ea] removed `ubx_connect_one` and `ubx_connect`. Use
  `ubx_connect_out`, `ubx_connect_in` and `ubx_connect_uni` instead.

- the port direction attrs `PORT_DIR_IN/OUT/INOUT` can be ommitted in
  the definition of block ports. They are automatically inferred from
  the presence of `in_type_name` / `out_type_name`.

- renamed `module_init/module_cleanup` to
  `UBX_MODULE_INIT/UBX_MODULE_CLEANUP`
