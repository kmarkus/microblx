# ubx/ramp

Ramp signal generator. Each step the current value is written to `out`,
then incremented by `slope`. The numeric type is configured at runtime
via the `type` config; array-valued signals are supported via
`data_len` (an independent ramp per element).

The ramp accumulates in the **native type**, and `start`/`slope` have
that type too: integer ramps count exactly over the full type range
(e.g. a `uint64_t` tick counter) and unsigned ramps wrap as expected.

## Configuration

| config     | type     | description                                                         |
|------------|----------|---------------------------------------------------------------------|
| `type`     | `char`   | ubx numeric type name of the output (mandatory, preinit)            |
| `data_len` | `long`   | vector length (default 1, preinit)                                  |
| `start`    | `<type>` | start value: scalar (broadcast) or per-element `[data_len]` (def 0) |
| `slope`    | `<type>` | increment per step: scalar (broadcast) or `[data_len]` (mandatory)  |
| `loglevel` | `int`    | optional log level                                                  |

Supported `type` values: `int8_t`..`int64_t`, `uint8_t`..`uint64_t`,
`float` and `double`.

`start` and `slope` are added by the preinit hook, once `type` is
known. `ubx-launch`, `ubx.block_create` and `ubx.do_configure` handle
this; with the C API, call `ubx_block_preinit()` before setting them.
`type` and `data_len` can't be changed after preinit.

Values are converted to the type by LuaJIT, without range checks. A
Lua number is a double, so use a `ULL`/`LL` literal for exact 64-bit
values beyond 2^53, e.g. `start = 1700000000000000001ULL`.

## Ports

The port is created at runtime with the configured `type`.

| port  | direction | type     | description        |
|-------|-----------|----------|--------------------|
| `out` | out       | `<type>` | current ramp value |

## Behaviour

- The first output equals `start`; the value is incremented *after*
  writing.
- The ramp is reset to `start` on every (re-)start.
