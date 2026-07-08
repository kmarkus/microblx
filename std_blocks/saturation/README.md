# ubx/saturation

Clamps an input signal element-wise between `lower_limits` and
`upper_limits`. The numeric type is configured at runtime via the
`type` config; input and output share that type, so the block is a
drop-in signal filter (compare `ubx/movavg`). Array-valued signals are
supported via `data_len`.

Unclamped values pass through byte-exact; clamped elements are set to
the respective limit (rounded to nearest for integer types).

## Configuration

| config         | type     | description                                                    |
|----------------|----------|----------------------------------------------------------------|
| `type`         | `char`   | ubx numeric type name of the signal (mandatory)                |
| `data_len`     | `long`   | vector length (default 1)                                      |
| `lower_limits` | `double` | lower bounds: scalar (broadcast) or per-element `[data_len]` (mandatory) |
| `upper_limits` | `double` | upper bounds: scalar (broadcast) or per-element `[data_len]` (mandatory) |
| `loglevel`     | `int`    | optional log level                                             |

`lower_limits[i] <= upper_limits[i]` is checked at init. Supported
`type` values are `int32_t`, `uint32_t`, `int64_t`, `uint64_t`,
`float` and `double`. Limits are given as `double` regardless of
`type`; note that integer values beyond 2^53 cannot be represented
exactly.

## Ports

Both ports are created at runtime with the configured `type`.

| port  | direction | type     | description      |
|-------|-----------|----------|------------------|
| `in`  | in        | `<type>` | input signal     |
| `out` | out       | `<type>` | saturated output |

## Example

```lua
{ name = "sat1", type = "ubx/saturation" },
-- ...
{ name = "sat1", config = {
     type = "double", data_len = 3,
     lower_limits = { -1, -2, -3 },
     upper_limits = 10,               -- scalar: applies to all elements
} },
```
