# ubx/movavg

A generic fixed-window **moving-average** (simple moving average, SMA)
filter. It keeps the last `window` scalar values received on the `in`
port and emits their arithmetic mean on the `out` port every step.

The numeric type is configured at runtime via the `type` config; input
and output share that type, so the block is a drop-in signal filter
(compare `ubx/saturation`). Averaging is performed internally in
`double`; for integer output types the result is rounded to nearest.

## Configuration

| config     | type   | description                                       |
|------------|--------|---------------------------------------------------|
| `type`     | `char` | ubx numeric type name of the signal (mandatory)   |
| `window`   | `long` | number of samples in the averaging window (≥ 1, mandatory) |
| `data_len` | `long` | vector length; averaged per element (default 1)   |
| `loglevel` | `int`  | optional log level                                |

With `data_len` > 1 the `in`/`out` ports are vectors and an independent
moving average is maintained per element (channel): each index is
filtered as its own scalar signal.

Supported `type` values are `int32_t`, `uint32_t`, `int64_t`,
`uint64_t`, `float` and `double`.

## Ports

Both ports are created at runtime with the configured `type`.

| port  | direction | type     | description                        |
|-------|-----------|----------|------------------------------------|
| `in`  | in        | `<type>` | input signal                       |
| `out` | out       | `<type>` | moving average of the window       |

## Behaviour

- Each step reads `in`, pushes it into the ring buffer (evicting the
  oldest sample once `window` is reached) and writes the average of the
  currently held samples to `out`.
- **Warm-up:** before `window` samples have been collected, the average
  is taken over however many samples have arrived so far, so `out` is
  valid from the very first step.
- **NODATA** on `in`: the step is skipped and no output is produced.
- The window is **cumulative across stop/start** and is only cleared on
  (re-)init.
- The average is computed by summing the window each step (exact, no
  running-sum drift); `window` is expected to be small.
