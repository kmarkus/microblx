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
| `tstats_output_rate`   | `double` | throttle rate for tstats port output                     |
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
| `sleep_mode`      | `int`                   | 0=OS sleep (default), 1=busy-wait; ignored with `SCHED_DEADLINE` |

## Ports — common

| port           | direction | type              | description                         |
|----------------|-----------|-------------------|-------------------------------------|
| `active_chain` | in        | `int`             | switch the active chain at runtime  |
| `tstats`       | out       | `struct ubx_tstat`| timing statistics (if enabled)      |

## Ports — ptrig only

| port                 | direction | type                    | description                                                        |
|----------------------|-----------|-------------------------|--------------------------------------------------------------------|
| `shutdown`           | in        | `int`                   | write any value to stop the thread                                 |
| `period`             | in        | `struct ptrig_period`   | change the trigger period at runtime                               |
| `sched_deadline`     | in        | `struct ptrig_deadline` | update `SCHED_DEADLINE` parameters at runtime (Linux ≥ 3.14)      |
| `deadline_throt_cnt` | out       | `uint64_t`              | cumulative count of SCHED_DEADLINE budget overruns (Linux ≥ 4.16) |

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
is therefore ignored.

If the chain execution exceeds `runtime_ns`, the kernel throttles the thread
for the remainder of the period and (on Linux ≥ 4.16) sends `SIGXCPU`. ptrig
catches this signal, logs a warning, and increments the counter on the
`deadline_throt_cnt` output port so applications can monitor overruns.

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
