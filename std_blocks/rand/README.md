# ubx/rand_*

Random number generator. Uses `drand48` / `lrand48` / `mrand48` internally. Outputs one sample per step.

Variants: `ubx/rand_double`, `ubx/rand_float`, `ubx/rand_uint32`, `ubx/rand_int32`.

## Configuration

| field  | type   | description                          |
|--------|--------|--------------------------------------|
| `seed` | `long` | seed for `srand48` (default: 0)      |

## Ports

| port  | direction | type | description        |
|-------|-----------|------|--------------------|
| `out` | out       | *T*  | random sample      |
