Run-time reconfiguration with liburcu
=====================================

2026-10-07, mk

Status: phase 1 (hot connect/disconnect) implemented.

Goal
----

Allow connecting/disconnecting ports, stopping/starting blocks and
changing selected configs while trigger threads are running, without
blocking, allocating or freeing in the RT threads.

What breaks today
-----------------

All of these are races between the control thread (Lua, C app) and
an RT thread (ptrig, luablock):

- `array_block_add()` (`libubx/ubx.c:1264`) `realloc`s the
  `in/out_interaction` array that `__port_read()`/`__port_write()`
  (`ubx.c:2504`, `ubx.c:2554`) iterate. A move of the array is a
  use-after-free in the RT thread.
- `array_block_rm()` (`ubx.c:1295`) swaps the last element into the
  removed slot in place. A concurrent reader can skip an iblock, see
  one twice, or still be inside the removed iblock's `read()` when it
  is stopped and cleaned up.
- `ubx_block_stop()` (`ubx.c:2311`) runs the `stop` hook while the
  state is still `ACTIVE`, so a concurrent `step()` (or iblock
  `read/write`) runs in parallel with `stop`. `block_state` is read
  and written without atomics.
- Configs: blocks cache data pointers in `init`/`start` (e.g. Kp in
  `std_blocks/pid/pid.c:58`) and use them in `step`. `set_config` from
  Lua (`ubx.lua:1451`) writes in place (torn reads for arrays, and for
  `double` on 32-bit ARM), and if the length changes
  `ubx_data_resize()` (`ubx.c:875`) `realloc`s, leaving the cached
  pointer dangling.
- Trigger chains (`struct ubx_chain`, `libubx/trig_utils.h:96`) cache
  the triggee array; changing a chain requires stopping the trigger.

Only the ptrig ports (`period`, `period_ns`, `sched_deadline`,
`active_chain`) are safe run-time knobs today, because they go through an
lfrb.

Why RCU fits
------------

The access pattern is RCU's textbook case: RT readers on every cycle,
rare non-RT writers. With RCU the read side is wait-free and does no
syscalls; the writer copies, publishes with `rcu_assign_pointer()`,
then waits for a grace period (`synchronize_rcu()`) or defers the
free (`call_rcu()`), both in the control thread. The RT thread never
calls `malloc`/`free`.

liburcu is LGPL-2.1 (linking from BSD-3 microblx is fine), packaged in
Debian (`liburcu-dev`) and in oe-core (`recipes-support/liburcu`,
used by lttng-ust), so meta-microblx needs no new layer.

Flavor choice
-------------

| flavor | registration          | read lock cost                          | writer side               |
|--------|-----------------------|-----------------------------------------|---------------------------|
| qsbr   | required, per thread  | none; thread calls `quiescent_state()`  | no IPIs                   |
| memb   | required, per thread  | compiler barrier                        | membarrier IPI per GP     |
| mb     | required, per thread  | `smp_mb` per lock/unlock                | no IPIs                   |
| bp     | lazy on first lock    | compiler barrier (mb w/o membarrier)    | membarrier, polls for GP  |

- An unregistered reader is invisible to the grace period, i.e. a
  silent use-after-free. microblx is a library: readers are ptrig,
  luablock, the Lua control thread stepping blocks, and any thread a
  user block or application creates. Forgetting registration in one
  of them is the most likely bug.
- **Proposal: `urcu-bp`.** Registration is automatic, read side is
  the same cost as memb when `sys_membarrier` is available. Call
  `rcu_read_lock(); rcu_read_unlock();` once at thread start (ptrig
  `thread_startup`, luablock) so the lazy registration (allocation)
  happens before the RT loop.
- Cost of memb/bp on the writer: `membarrier(PRIVATE_EXPEDITED)` sends
  an IPI to every CPU currently running a thread of the process, i.e.
  also to isolated RT cores. That is a few us of jitter once per
  reconfiguration, not per cycle. Irrelevant on the single-core BBB.
  If that is not acceptable, `mb` trades it for two barriers per read
  section; qsbr avoids both but makes registration and online/offline
  handling (offline around `clock_nanosleep`) mandatory in every
  reader thread.

Design
------

1. **Read-side sections** (nestable):
   - coarse: `ubx_cblock_step()` wraps `b->step()` in
     `rcu_read_lock/unlock`. This covers all steps regardless of which
     thread triggers them.
   - fine: `__port_read()`/`__port_write()` wrap the interaction
     iteration, so port access from outside a step (e.g. Lua
     `port_read`) is also covered.
   - Nothing obtained inside a section may be kept after it. Port
     reads copy into the caller's buffer, so the port API already
     satisfies this.

2. **Connections** (`ubx.c`): `array_block_add/rm` become copy-update:
   allocate new array, copy, modify, `rcu_assign_pointer()`, free the
   old one after a grace period. Readers use `rcu_dereference()` on
   `in/out_interaction`. A node-level mutex serializes writers (all
   structural changes, non-RT only). Connect at run-time is then:
   create/init/start iblock (allocates, non-RT), connect (publish).
   Disconnect: unpublish, `synchronize_rcu()`, stop/cleanup/rm iblock.

3. **Block stop**: `ubx_block_stop()` sets `block_state = INACTIVE`
   (atomic store) *before* the hook, then `synchronize_rcu()`, then
   calls `stop`. After this no `step`/`read`/`write` of the block is
   running or will start. Readers load `block_state` with
   `CMM_LOAD_SHARED`. This makes stopping an iblock that is still
   connected, or a cblock that is still in a running chain, safe.
   Same for `ubx_block_cleanup()`.

4. **Live configs** (opt-in, new attribute `CONFIG_ATTR_LIVE`):
   - Writer: `ubx_config_set_live(c, d)` allocates a new `ubx_data_t`,
     copies `d`, `rcu_assign_pointer(c->value, new)`, `call_rcu()`
     drops the old one (respecting `refcnt`). Min/max are checked
     before publishing.
   - Block: must not cache the data pointer. In `step` it calls a new
     `cfg_getlive_<type>(c, &ptr)` (generated like `cfg_getptr_*`),
     valid until `step` returns. One pointer load per config per step.
   - Non-live configs: `set_config` on a block that is not `PREINIT`
     or `INACTIVE` is rejected (today it silently races). This is a
     behavior change, but matches the documented contract that configs
     are applied in `init`/`start` (`docs/blocks.md:110`).
   - Lua `set_config` dispatches to the live path for live configs.

5. **Trigger chains** (later): `ptrig` holds an RCU pointer to a fully
   built `struct ubx_chain` (triggees + `blk_tstats`). A new chain is
   built and `ubx_chain_init()`ed in the control thread and swapped
   in; the RT loop dereferences it once per cycle. Stats of the old
   chain are lost or merged in the free callback. Until then,
   `active_chain` switching between pre-built chains remains the RT-safe
   way.

6. **call_rcu worker**: liburcu runs callbacks in a helper thread
   (`free()` happens there). Pin it to non-RT CPUs
   (`create_call_rcu_data()` with a CPU, or set its affinity), and
   call `rcu_barrier()` in `ubx_node_cleanup()` before unloading
   modules, so no callback runs into unloaded code.

RT impact
---------

- Per cycle: one compiler barrier and two TLS stores per
  `rcu_read_lock/unlock` pair (bp/memb with membarrier), one extra
  pointer load per port access and per live config. No syscalls, no
  locks, no allocation.
- Per reconfiguration: one membarrier IPI per grace period on CPUs
  running process threads.
- `synchronize_rcu()` in the control thread waits until all running
  read sections finish. With phase 1 (sections only in
  `__port_read/write`) that is the longest port access in progress,
  with a coarse section around `step` up to one trigger period. An
  iblock `read`/`write` that blocks (I/O) delays the writer, never the
  reader; a blocking `ubx/mqueue` is therefore rejected with urcu.
- `synchronize_rcu()` must not be called from inside a step (deadlock
  with itself): reconfiguration from a luablock step must use
  `call_rcu()`, or the API returns an error when called inside a read
  section.

Grace periods and stuck readers
-------------------------------

urcu-bp: each reader has a TLS counter, nesting count in the low bits.
The outermost `rcu_read_lock()` copies the global phase bit into it,
`rcu_read_unlock()` decrements the nesting. `synchronize_rcu()` does a
membarrier, waits until no reader is nested with the old phase, flips
the phase, waits again, then a final membarrier. Waiting busy-loops
100 times, then polls every 10 ms (`RCU_SLEEP_DELAY_MS`), so a grace
period takes at least as long as the longest running read section
(phase 1: port access, later: step), and up to about 10 ms on top.

A thread stopped *outside* a read section (sleeping in ptrig, or
stopped in gdb between steps) has nesting 0 and does not block grace
periods. A thread stopped *inside* one (breakpoint in a `step()`)
blocks `synchronize_rcu()` until it continues. Nothing becomes
incorrect: the writer just waits, and `call_rcu()` callbacks queue
up. With gdb in all-stop mode the writer is stopped as well; only
non-stop mode, or a thread stuck in a blocking step, makes the
writer visibly hang.

Phase 1 writers call the blocking `synchronize_rcu()`, so a stuck
reader hangs every connect and disconnect. Later, to keep the control
thread responsive, writers could use
`start_poll_synchronize_rcu()`/`poll_state_synchronize_rcu()` with a
timeout: on timeout, the operation returns `EBUSY` and the old data
is freed through `call_rcu()`. liburcu does not report which reader
is blocking. For that, trigger threads would record "in step since
<ts>, block <name>" for the log message.

Measurement: membarrier IPIs on the AM625 (BeaglePlay)
------------------------------------------------------

2026-10-07, 6.12.57-ti PREEMPT_RT, `isolcpus=3 nohz_full=3`.
`HOTC_MEASURE=1`: trig1 (SCHED_FIFO 80, CPU 3, 1 ms) runs a C only
chain (`ramp_double` -> lfrb -> `math_double` sin, N=2000, ~320 us)
that is not rewired; the main thread and the rewirer churn trig2/3
at about 700 ops/s. 30 s per run, 3 alternating rounds.

| mode               | fn-call IPIs CPU 3 | step avg        | step max        | wakeup lat max  |
|--------------------|--------------------|-----------------|-----------------|-----------------|
| idle (no churn)    | 0                  | 318 us          | 351 us          | 58 us           |
| bp + membarrier    | 57k, 56k, 62k      | 342, 342, 341   | 411, 400, 401   | 101, 87, 104 us |
| bp, membarrier off | 0                  | 336, 335, 336   | 378, 377, 403   | 95, 90, 91 us   |

- IPIs only reach CPU 3 while trig1 runs (~1.9 per cycle here at a
  32% duty cycle); a sleeping RT thread gets none.
- membarrier costs the RT thread a consistent ~6 us avg step time
  (~3 us per IPI incl. entry/exit and cache effects) and ~20 us on
  the step max. IPI handler alone (`trace-cmd` ipi_entry..ipi_exit):
  median 1 us, p99 7 us, max 10 us.
- The writer is ~10% faster without membarrier.
- The churn on the other cores raises wakeup latency and step time in
  both modes (shared L2, memory); that part is not RCU specific.
- At a realistic 10 connect/disconnect ops/s (rewirer off, 120 s
  runs, 2 rounds) there is no measurable difference: ~16 IPIs/s hit
  CPU 3 (1.6% of cycles), step avg 320/321 us and step max
  375/373 us (membarrier) vs 371/364 us (off) vs 390 us idle; wakeup
  latency max varies 66..109 us across all modes, idle included.
  Decision: keep bp with membarrier.
- An earlier run with a Lua checker in trig1 showed ~1.2 ms step
  maxima from LuaJIT itself and hid all of this.

Alternatives
------------

- **Status quo + ports**: make tunables input ports (as ptrig does for
  `period`). RT-safe today, no dependency, but a copy per cycle and no
  structural changes (connections, chains, stopping blocks).
- **Hand-rolled epoch**: each trigger thread bumps a counter per cycle,
  the writer waits until all counters advanced or the thread is
  inactive. ~50 lines, but misses non-trigger readers (Lua stepping,
  user threads), which is exactly the hard part liburcu solves.
- **Stop the trigger, change, restart**: correct and simple, but loses
  cycles; unacceptable for running control loops.

Build option and licensing
--------------------------

- `ENABLE_URCU` (default OFF) links `liburcu-bp` (>= 0.11). All urcu
  use is behind one internal header (`libubx/internal/ubx_rcu.h`) with
  macros (`UBX_RCU_READ_LOCK`, `UBX_RCU_DEREF`, `UBX_RCU_ASSIGN`,
  `UBX_RCU_SYNC`, ...); without urcu they are no-ops / plain loads
  and stores. The copy-update code is the same in both builds.
  Reported as `urcu=yes|no` by `ubx_build_config()`, checked with
  `ubx_hot_connect()`.
- The read side is inlined (`_LGPL_SOURCE`): as library calls, read
  lock/unlock cost ~4x per port access. MPL-2.0 allows combining
  with the LGPL-2.1 code, but libubx then depends on the liburcu
  version it was built against and must be rebuilt on updates.
  Blocks only call `ubx_*` functions and are not affected.
- The control mutex is a static mutex in `ubx.c`, so `ubx_node_t`
  (and its FFI cdef) is unchanged.

Block authors
-------------

- Port API unchanged. Blocks must already handle unconnected ports
  (read returns 0); with hot connect this can change between steps.
- Connect/disconnect only from non-RT threads: they take the control
  mutex, allocate, and wait for a grace period. Calling them from a
  step is fine (e.g. lsdb-intf in its luablock thread), as long as
  the step is not inside a read section: `array_block_add/rm` check
  `urcu_bp_read_ongoing()` and return `EWRONG_STATE` instead of
  deadlocking. This matters once `step` gets a coarse section.
- Lua introspection (`port_conns_totab`) walks the interaction arrays
  between `ubx_rcu_read_lock()` and `ubx_rcu_read_unlock()`.
- While triggers run: disconnect before stopping an iblock. With
  triggers stopped first (as `ubx_node_clear()` does) any order is
  fine, as today.
- Thread registration is optional with bp. Registration happens on the
  first port access of a thread and allocates; RT threads should call
  `ubx_rcu_thread_init()` before their loop (ptrig and luablock will).
- Never cache `in/out_interaction`; only `ubx.lua` port introspection
  reads them, from the control thread.

Plan
----

Phase 1: hot connect/disconnect (this branch)

1. Add `liburcu-bp` as optional CMake dependency (`ENABLE_URCU`),
   no-op macros without it.
2. Read sections only in `__port_read/write` (fine-grained); no
   section around `step`.
3. Copy-update for `in/out_interaction` under a global control mutex.
   Disconnect waits for the grace period before returning, so the
   iblock can be stopped and removed right after.
4. Stress test `tests/test_hot_connect.lua`: N ptrig threads step
   ramp writers and checker luablocks; the main thread connects and
   disconnects (create/start/stop/rm lfrbs) at random, a luablock
   thread concurrently rewires preallocated lfrbs on the same ports.
   Checkers validate every value. Scale with `HOTC_ITER`, run under
   ASan. On a build without urcu (with the disconnect guard
   bypassed) it segfaults 3/3 and ASan reports a use-after-free in
   `__port_read` (negative control, done once). TSan is not useful: liburcu is not
   instrumented and orders via membarrier.

Later

5. Coarse section around `step` + `ubx_block_stop/cleanup` ordering
   fix (stopping cblocks in a running chain).
6. Live configs, chain swapping.
