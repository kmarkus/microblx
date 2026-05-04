# ubx/ramp_*

Ramp signal generator. Each step the current value is written to `out`, then incremented by `slope`. Supports array-valued signals via `data_len`.

Variants: `ubx/ramp_double`, `ubx/ramp_float`, `ubx/ramp_int8`, `ubx/ramp_int16`, `ubx/ramp_int32`, `ubx/ramp_int64`.

## Configuration

| field      | type | description                              |
|------------|------|------------------------------------------|
| `slope`    | *T*  | increment per step (required)            |
| `start`    | *T*  | initial value (default: 0)               |
| `data_len` | `long` | array length (default: 1)              |

## Ports

| port  | direction | type | description      |
|-------|-----------|------|------------------|
| `out` | out       | *T*  | current ramp value |
