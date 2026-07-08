# ubx/rand

Random number generator. Each step a fresh random sample (a vector for
`data_len` > 1) is written to `out`. The numeric type is configured at
runtime via the `type` config:

- **floating-point** types: uniform in [0, 1)
- **integer** types: uniform over the full type range

The PRNG state is **per instance** (`erand48`/`jrand48` family):
instances do not race on the global `drand48` state across trigger
threads, seeding one block does not re-seed the others, and each
instance's sequence is reproducible from its `seed`. The state is
seeded at init exactly like `srand48`, so a `double` instance
reproduces the legacy `ubx/rand_double` sequence for the same seed.

## Configuration

| config     | type   | description                                     |
|------------|--------|-------------------------------------------------|
| `type`     | `char` | ubx numeric type name of the output (mandatory) |
| `data_len` | `long` | vector length (default 1)                       |
| `seed`     | `long` | seed of this instance's PRNG (default 0)        |
| `loglevel` | `int`  | optional log level                              |

Supported `type` values: `int8_t`..`int64_t`, `uint8_t`..`uint64_t`,
`float` and `double`.

## Ports

The port is created at runtime with the configured `type`.

| port  | direction | type     | description   |
|-------|-----------|----------|---------------|
| `out` | out       | `<type>` | random sample |

## Behaviour

- The sequence continues across stop/start; only re-init restarts it
  from `seed`.

## Legacy variants: ubx/rand_*

The compile-time typed variants `ubx/rand_double`, `ubx/rand_float`,
`ubx/rand_uint32`, `ubx/rand_int32` predate `ubx/rand` and are kept for
backwards compatibility. They use the *process-global* `drand48` state
(not thread-safe across instances; `seed` affects all of them) and are
scalar-only. Note the integer ranges differ: legacy `rand_uint32` is
uniform in [0, 2^31) via `lrand48`, while `ubx/rand` with `uint32_t` is
full-range. Prefer `ubx/rand` in new compositions.
