# ubx/trig, ubx/ptrig

Trigger blocks that call a sequence of blocks (`chain0`, `chain1`, …) each step.

- **`ubx/trig`** — passive, activity-less; must be called by another trigger or the application.
- **`ubx/ptrig`** — active; runs its own POSIX thread at a configurable period and scheduling policy.

Both support multiple trigger chains, per-block timing statistics, and runtime chain switching.

## Configuration — common

| field                  | type     | description                                              |
|------------------------|----------|----------------------------------------------------------|
| `chain0` … `chainN`    | `struct ubx_triggee[]` | ordered list of `{ b, num_steps, every }` entries |
| `num_chains`           | `int`    | number of chains (default: 1)                            |
| `tstats_mode`          | `int`    | 0=off, 1=global only, 2=per block (default: 0)           |
| `tstats_profile_path`  | `char`   | directory to write timing stats file to                  |
| `tstats_output_rate`   | `double` | min seconds between tstats port outputs (0: only emit on stop) |
| `tstats_skip_first`    | `int`    | skip N steps before collecting stats                     |
| `loglevel`             | `int`    | optional log level                                       |

## Configuration — ptrig only

| field             | type                    | description                                             |
|-------------------|-------------------------|---------------------------------------------------------|
| `period`          | `struct ptrig_period`   | trigger period `{ sec, usec }` (required)               |
| `sched_policy`    | `char`                  | scheduling policy: `SCHED_OTHER` (default), `SCHED_FIFO`, `SCHED_RR`, `SCHED_DEADLINE` (Linux ≥ 3.14) |
| `sched_priority`  | `int`                   | thread priority; unused with `SCHED_DEADLINE`           |
| `sched_deadline`  | `struct ptrig_deadline` | `SCHED_DEADLINE` parameters `{ runtime_ns, deadline_ns, period_ns }`; `deadline_ns` and `period_ns` default to the `period` config value when 0 (Linux ≥ 3.14) |
| `affinity`        | `int[]`                 | list of CPUs for pthread affinity                       |
| `stacksize`       | `size_t`                | thread stack size                                       |
| `thread_name`     | `char`                  | thread name shown in debuggers (default: block name)    |
| `autostop_steps`  | `int64_t`               | stop automatically after N steps                        |
| `sleep_mode`      | `int`                   | 0=OS sleep (default), 1=busy-wait, 2=hybrid; ignored with `SCHED_DEADLINE` |
| `busy_slack_ns`   | `int64_t`               | `sleep_mode=2`: duration to busy-wait before the deadline [ns] (default: 50000) |
| `timerslack_ns`   | `int64_t`               | thread timer slack [ns]; 0 (default) leaves it unchanged |

## Ports — common

| port           | direction | type              | description                         |
|----------------|-----------|-------------------|-------------------------------------|
| `active_chain` | in        | `int`             | switch the active chain at runtime  |
| `tstats`       | out       | `struct ubx_tstat`| timing statistics (if enabled)      |

## Ports — ptrig only

| port                 | direction | type                    | description                                                        |
|----------------------|-----------|-------------------------|--------------------------------------------------------------------|
| `shutdown`           | in        | `int`                   | write any value to stop the thread                                 |
| `period`             | in        | `struct ptrig_period`   | change the trigger period at runtime (no effect under SCHED_DEADLINE — use `sched_deadline`) |
| `period_ns`          | in        | `int64_t`               | change the trigger period [ns] at runtime (no effect under SCHED_DEADLINE — use `sched_deadline`) |
| `sched_deadline`     | in        | `struct ptrig_deadline` | update `SCHED_DEADLINE` parameters at runtime (Linux ≥ 3.14)      |
| `deadline_throt_cnt` | out       | `uint64_t`              | cumulative count of SCHED_DEADLINE budget overruns (Linux ≥ 4.16) |
| `overrun_cnt`        | out       | `uint64_t`              | cumulative count of missed trigger deadlines; counts skipped periods in sleep/busy-wait modes only (stays 0 with SCHED_DEADLINE — use `deadline_throt_cnt` there) |

## Overrun handling

A trigger deadline is missed when the chain (plus the wakeup latency of
the sleep mode) takes longer than the period. ptrig then **drops** the
missed tick(s) and resumes on the original grid: it never triggers
back-to-back to catch up, and it never re-anchors the grid. The phase of
every subsequent trigger is unchanged, and the number of dropped
triggers is added to `overrun_cnt`.

At stop, ptrig logs the total as a standalone line next to the timing
statistics:

```
OVERRUNS: 7 missed trigger deadline(s)
```

The count is cumulative over the lifetime of the block, matching the
`overrun_cnt` output port. It is deliberately not part of `struct
ubx_tstat`: a missed deadline belongs to the trigger, not to a block's
execution time.

For a 10ms period with a chain that once takes 15ms, the triggers land
at 0, 10, **(chain runs 20→35)**, 40, 50ms — the 30ms tick is dropped,
`overrun_cnt` is 1, and 40ms onwards is back on the original grid.

The grid is only re-anchored when the block is (re)started, so an
inactive interval is never "caught up" either.

Note that the wakeup latency counts against the period budget: an
overrun is declared when the chain exceeds `period - latency`. That
makes `sleep_mode=0` declare an overrun slightly sooner than modes 1
and 2 for the same chain (on an RT-tuned ARM SoC, ~12µs sooner).

## Sleep modes

`sleep_mode` selects how ptrig waits for the next period:

| mode | name   | accuracy      | CPU cost        |
|------|--------|---------------|-----------------|
| 0    | sleep  | OS wakeup latency | none        |
| 1    | busy   | clock resolution  | 100% of a core |
| 2    | hybrid | clock resolution  | `busy_slack_ns / period` |

The **hybrid** mode sleeps for `period - busy_slack_ns` via
`clock_nanosleep(2)` and busy-waits the remaining `busy_slack_ns`. The
sleep gives up the CPU for the bulk of the period; the busy-wait phase
absorbs the OS wakeup latency, so the trigger fires with busy-wait
accuracy at a fraction of the CPU cost.

For this to work, **`busy_slack_ns` must exceed the platform's
worst-case wakeup latency**. If a sleep overshoots by more than
`busy_slack_ns`, the busy-wait phase never runs and the accuracy
degrades to that of `sleep_mode=0` — the mode fails soft, but it fails.
Size the value from the worst-case wakeup latency of the target (e.g.
`cyclictest -m -p80` max), not from the median. The default of 50µs
covers an RT-tuned ARM SoC; on a tuned x86 with deep C-states disabled
10-20µs is enough.

The busy-wait phase spins at the thread's scheduling priority, so under
`SCHED_FIFO`/`SCHED_RR` it will keep lower-priority tasks off that CPU for
up to `busy_slack_ns` every period. ptrig warns at init when
`busy_slack_ns` exceeds 10% of the period.

## Timer slack

Linux applies a default timer slack of 50µs to the hrtimer expiry of
non-realtime threads, which shows up directly as trigger lateness under
`SCHED_OTHER`. Realtime policies (`SCHED_FIFO`, `SCHED_RR`,
`SCHED_DEADLINE`) are exempt and ignore the setting.

`timerslack_ns` sets the ptrig thread's slack via
`prctl(PR_SET_TIMERSLACK)`; the default of 0 leaves the inherited value
alone. Setting it to 1 is worthwhile for any `SCHED_OTHER` ptrig that
cares about accuracy, in `sleep_mode` 0 and 2 alike.

## SCHED_DEADLINE

When `sched_policy` is set to `"SCHED_DEADLINE"`, ptrig uses the Linux
EDF scheduler instead of the standard POSIX priority-based policies.
Three timing parameters must be provided via the `sched_deadline` config:

- **`runtime_ns`** — worst-case execution time (WCET) budget per period in nanoseconds (mandatory)
- **`deadline_ns`** — relative deadline in nanoseconds; 0 = use `period_ns`
- **`period_ns`** — scheduling period in nanoseconds; 0 = derive from the `period` config

The kernel enforces `runtime_ns ≤ deadline_ns ≤ period_ns`; ptrig validates
this at init and logs a clear error if the constraint is violated.

When active, `sched_yield(2)` replaces the normal sleep after each chain
trigger — this signals the kernel that the current job activation is done
and lets it replenish the budget at the next period boundary. `sleep_mode`
is therefore ignored (a non-zero value is rejected at init).

If the chain execution exceeds `runtime_ns`, the kernel throttles the thread
for the remainder of the period and (on Linux ≥ 4.16) sends `SIGXCPU`. ptrig
catches this signal and increments the counter on the `deadline_throt_cnt`
output port so applications can monitor overruns. As with `overrun_cnt`, each
overrun is logged at debug level and the cumulative total is logged as a
warning on stop.

> **Note:** `SCHED_DEADLINE` requires `CAP_SYS_NICE`. Do not use
> `setcap cap_sys_nice+ep` on the interpreter — file capabilities set
> the `AT_SECURE` bit on exec, which causes sd-bus to ignore
> session-bus environment variables and breaks `-dbus`. Grant the
> capability via `sudo capsh` with ambient capabilities instead (see
> `examples/usc/pid/run-pid.sh` for a working example); this does not
> require any file capability on the binary.
>
> **CPU affinity:** A DEADLINE thread's `cpus_allowed` mask must be a
> superset of its scheduling root domain. Without cpuset isolation the
> only root domain covers all online CPUs, so combining `affinity` with
> `SCHED_DEADLINE` will fail with `EPERM`. To pin EDF tasks to a CPU
> subset, create an isolated cpuset partition (`cpuset.cpus` plus
> `cpuset.sched_load_balance=0`, see `cpuset(7)`) so a matching root
> domain exists; the `affinity` config can then be set accordingly.
