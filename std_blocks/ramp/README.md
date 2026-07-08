# ubx/ramp

Ramp signal generator. Each step the current value is written to `out`,
then incremented by `slope`. The numeric type is configured at runtime
via the `type` config; array-valued signals are supported via
`data_len` (an independent ramp per element).

The ramp accumulates in the **native type**: a per-type increment
kernel is selected once at init. Integer ramps therefore count exactly
over the full type range (no 2^53 double mantissa limit — e.g. a
`uint64_t` tick counter) and unsigned ramps wrap as expected. The
`start`/`slope` configs are given as `double` and are range-checked and
converted (integers rounded to nearest) once at init.

## Configuration

| config     | type     | description                                                   |
|------------|----------|---------------------------------------------------------------|
| `type`     | `char`   | ubx numeric type name of the output (mandatory)               |
| `data_len` | `long`   | vector length (default 1)                                     |
| `start`    | `double` | start value: scalar (broadcast) or per-element `[data_len]` (default 0) |
| `slope`    | `double` | increment per step: scalar (broadcast) or per-element `[data_len]` (mandatory) |
| `loglevel` | `int`    | optional log level                                            |

Supported `type` values: `int8_t`..`int64_t`, `uint8_t`..`uint64_t`,
`float` and `double`. Config values out of range for the chosen type
are refused at init. Note that `double` configs cannot express integer
values beyond 2^53 exactly (the ramp itself counts exactly from
wherever it starts).

## Ports

The port is created at runtime with the configured `type`.

| port  | direction | type     | description        |
|-------|-----------|----------|--------------------|
| `out` | out       | `<type>` | current ramp value |

## Behaviour

- The first output equals `start`; the value is incremented *after*
  writing.
- The ramp is reset to `start` on every (re-)start.

## Legacy variants: ubx/ramp_*

The compile-time typed variants `ubx/ramp_double`, `ubx/ramp_float`,
`ubx/ramp_int8` ... `ubx/ramp_uint64` (one module each, configs typed
*T*, no scalar broadcast) predate `ubx/ramp` and are kept for backwards
compatibility. Prefer `ubx/ramp` in new compositions.
