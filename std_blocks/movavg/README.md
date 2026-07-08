# ubx/movavg

A generic fixed-window **sliding filter**. It keeps the last `window`
values received on the `in` port and emits an aggregate over them on
the `out` port every step. The aggregate is selected by the `mode`
config: **mean** (simple moving average, the default), **median**,
**min** or **max**.

The numeric type is configured at runtime via the `type` config; input
and output share that type, so the block is a drop-in signal filter
(compare `ubx/saturation`). The filter is computed internally in
`double`; for integer output types the result is rounded to nearest.

## Configuration

| config     | type   | description                                       |
|------------|--------|---------------------------------------------------|
| `type`     | `char` | ubx numeric type name of the signal (mandatory)   |
| `window`   | `long` | number of samples in the window (≥ 1, mandatory)  |
| `data_len` | `long` | vector length; filtered per element (default 1)   |
| `mode`     | `char` | `mean` (default), `median`, `min` or `max`        |
| `loglevel` | `int`  | optional log level                                |

With `data_len` > 1 the `in`/`out` ports are vectors and an independent
filter is maintained per element (channel): each index is filtered as
its own scalar signal.

Supported `type` values are `int32_t`, `uint32_t`, `int64_t`,
`uint64_t`, `float` and `double`.

## Ports

Both ports are created at runtime with the configured `type`.

| port  | direction | type     | description                        |
|-------|-----------|----------|------------------------------------|
| `in`  | in        | `<type>` | input signal                       |
| `out` | out       | `<type>` | window aggregate                   |

## Modes

- **mean** — the arithmetic mean (SMA); good general smoothing.
- **median** — robust to outliers: spikes shorter than `window/2` are
  removed entirely rather than smeared into neighboring samples. For an
  even number of samples the mean of the two middle values is used.
- **min** / **max** — sliding envelope tracking.

## Behaviour

- Each step reads `in`, pushes it into the ring buffer (evicting the
  oldest sample once `window` is reached) and writes the aggregate of
  the currently held samples to `out`.
- **Warm-up:** before `window` samples have been collected, the
  aggregate is taken over however many samples have arrived so far, so
  `out` is valid from the very first step.
- **NODATA** on `in`: the step is skipped and no output is produced.
- The window is **cumulative across stop/start** and is only cleared on
  (re-)init.
- The aggregate is recomputed over the window each step (mean: exact
  sum, no running-sum drift; median: insertion sort into a preallocated
  scratch buffer); `window` is expected to be small.
