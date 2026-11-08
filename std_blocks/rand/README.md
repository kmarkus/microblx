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
gives the `drand48` sequence for the same seed.

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
