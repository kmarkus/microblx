<div align="center">

**[microblx](../README.md) · [Install](../README.md#installing) · [Quickstart](../README.md#quickstart) · [Developing blocks](../README.md#developing-blocks) · [Composing systems](../README.md#composing-systems) · [Standard blocks](../README.md#standard-blocks) · [Block reference](blocks.md) · [usc reference](usc.md) · Runtime reconfiguration**

</div>

# Runtime reconfiguration

What can be changed while triggers are running, what can't, and the
constraints that must hold.

## What can change while running

| change                                   | how                                                           | requires      |
|------------------------------------------|---------------------------------------------------------------|---------------|
| connect/disconnect ports                 | `ubx.connect`, `ubx.disconnect`, D-Bus `Connect`/`Disconnect` | `ENABLE_URCU` |
| trigger period                           | ptrig `period`, `period_ns`, `sched_deadline` ports           | -             |
| active trigger chain                     | `active_chain` port of `ubx/ptrig` and `ubx/trig`             | -             |
| start/stop a trigger                     | block state of the trigger (stops its whole chain)            | -             |
| start/stop blocks not in a running chain | block state                                                   | -             |

## What can't

- **configs**: blocks read them in `init`/`start` and cache pointers
  to the data. Setting a config of an active block races with its
  `step`, and a length change frees the buffer the block still
  uses. Stop the block (or its trigger), set it, start again. Values
  that must change at run time belong on an input port, like the
  ptrig `period`.
- **trigger chain contents**: stop the trigger first. To switch
  between known configurations, configure several chains and switch
  with the `active_chain` port.
- **stopping a single block in a running chain**: its `stop` hook
  would run concurrently with its `step`. Stop the trigger first.
- **adding, removing or resizing ports and configs**: resizing ports
  only in state `preinit` (enforced). Adding and removing ports and
  configs only in the life-cycle hooks, which the control thread
  runs: `preinit`, `init`, `start` and their counterparts `stop`,
  `cleanup`, `preexit`. Never in `step` (also not via luablock
  `exec_str`): `ubx.connect`, `ubx.disconnect` and
  `ubx.block_unload` walk the port lists of all blocks and could
  read a port while it is freed. Not checked.
- **connections without `ENABLE_URCU`**: only while the blocks
  involved are stopped. `ubx.disconnect` refuses with "stop it
  first", `connect` does not check.

## Hot connect/disconnect

Build with `-DENABLE_URCU=ON`, which requires liburcu-bp >= 0.11
(`apt install liburcu-dev`; in Yocto `liburcu` from oe-core). Check
the build:

```sh
$ ubx-launch --version | grep urcu
  urcu:                yes
```

Port read/write then run in RCU read-side sections. A connect or
disconnect publishes a changed copy of the port's connection array
and frees the old one after a *grace period*, i.e. once no thread
can still be in a port access that saw the old one.

### Using ubx.lua: safe

`ubx.connect`, `ubx.disconnect` and everything built on them
(e.g. the D-Bus interface of `lsdb-intf`) handle the details:
the iblock of a new connection is started before it is connected,
and an iblock is disconnected before it is stopped and removed.
Connection introspection (`ubx.port_conns_totab`,
`ubx.build_conntab`) is safe against concurrent changes.

```lua
-- while the trigger is running
assert(ubx.connect(nd, "ramp", "out", "sink", "in", "ubx/lfrb", { buffer_len=4 }))
assert(ubx.disconnect(nd, "ramp", "out", "sink", "in"))
```

### Using the C API: up to the author

`ubx_ports_connect`, `ubx_port_connect_in/out` and the
`*_disconnect*` counterparts only update the connection. The order
around them is up to the caller:

1. connect: create, init and **start** the iblock, then connect it.
2. disconnect: disconnect (returns after the grace period), then
   stop, clean up and remove the iblock.

Code that walks `port->in_interaction` or `port->out_interaction`
directly must do so between `ubx_rcu_read_lock()` and
`ubx_rcu_read_unlock()`, must not keep any pointer from the array
after the unlock, and must always reach the unlock: a section left
open blocks every later grace period, so all connects and
disconnects hang.

### Constraints

- **Non-RT threads only.** Connect and disconnect take a global
  mutex, allocate memory, and wait for a grace period (up to the
  longest port read/write in progress plus ~10 ms). Called from an
  RT block's `step` that is how long the step blocks.
- **iblock `read`/`write` must not block.** They run inside the read
  section, so a blocking one stalls every connect and disconnect of
  the process until it returns.
- **Ports may change between steps.** Blocks must handle a port
  that becomes unconnected (`read_*` returns 0) or connected.
- **One control thread for creating and removing blocks.** The global
  mutex serializes connection changes from several threads, but
  block creation and removal (`ubx.block_create`, `ubx.block_rm`,
  ...) are not thread safe.
- **RT threads should call `ubx_rcu_thread_init()`** before their
  loop (see below).
- Connecting from inside a read section, i.e. from an iblock's
  `read`/`write` hook or between `ubx_rcu_read_lock/unlock`, would
  deadlock on the grace period. The call fails with `EWRONG_STATE`
  instead.

### Threads

liburcu-bp registers each thread on its first port access. That
one-time registration allocates and takes liburcu's registry mutex,
which a writer holds while it waits for a grace period. For an RT
thread this means a one-time delay of up to a grace period, with
priority inversion behind a non-RT writer. Avoid it by calling
`ubx_rcu_thread_init()` at thread start, before the RT loop.
`ubx/ptrig` and `ubx/luablock` do this; blocks that create their
own threads should do the same. Without `ENABLE_URCU` it is a no-op.

## Cost

- **Per port read/write**: two calls into liburcu-bp (read lock and
  unlock), no locks, no syscalls.
- **Per connect/disconnect**: liburcu-bp uses `membarrier(2)`, which
  interrupts (IPI) the CPUs that are running a thread of the process
  at that moment, i.e. an RT thread only while it is in a step.
  Measured on a BeaglePlay (AM625, PREEMPT_RT): at 10
  connects/disconnects per second, ~16 IPIs/s on the RT core with no
  measurable effect on step time or wakeup latency. Under 700 ops/s
  the average step time grew by ~6 us. Details:
  [design note](dev/008-urcu-runtime-reconfig.md).
