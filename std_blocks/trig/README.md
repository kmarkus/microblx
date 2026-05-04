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

| field             | type                  | description                                             |
|-------------------|-----------------------|---------------------------------------------------------|
| `period`          | `struct ptrig_period` | trigger period `{ sec, usec }` (required)               |
| `sched_priority`  | `int`                 | pthread priority                                        |
| `sched_policy`    | `char`                | `SCHED_OTHER`, `SCHED_FIFO`, or `SCHED_RR`             |
| `affinity`        | `int[]`               | list of CPUs for pthread affinity                       |
| `stacksize`       | `size_t`              | thread stack size                                       |
| `thread_name`     | `char`                | thread name shown in debuggers (default: block name)    |
| `autostop_steps`  | `int64_t`             | stop automatically after N steps                        |
| `sleep_mode`      | `int`                 | 0=OS sleep (default), 1=busy-wait                       |

## Ports — common

| port           | direction | type              | description                         |
|----------------|-----------|-------------------|-------------------------------------|
| `active_chain` | in        | `int`             | switch the active chain at runtime  |
| `tstats`       | out       | `struct ubx_tstat`| timing statistics (if enabled)      |

## Ports — ptrig only

| port       | direction | type                  | description                          |
|------------|-----------|-----------------------|--------------------------------------|
| `shutdown` | in        | `int`                 | write any value to stop the thread   |
| `period`   | in        | `struct ptrig_period` | change the trigger period at runtime |
