# ubx/saturation_*

Clamps an input signal element-wise between `lower_limits` and `upper_limits`. Supports array-valued signals via `data_len`.

Variants: `ubx/saturation_double`, `ubx/saturation_float`, `ubx/saturation_int32`, `ubx/saturation_int64`.

## Configuration

| field          | type | description                              |
|----------------|------|------------------------------------------|
| `lower_limits` | *T*  | per-element lower bound (required)       |
| `upper_limits` | *T*  | per-element upper bound (required)       |
| `data_len`     | `long` | array length (default: 1)              |

## Ports

| port  | direction | type | description     |
|-------|-----------|------|-----------------|
| `in`  | in        | *T*  | input signal    |
| `out` | out       | *T*  | saturated output |
