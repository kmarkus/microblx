# ubx/ewma

A generic **exponentially-weighted moving-average** (EWMA, first-order
exponential smoothing) filter:

    y += alpha * (x - y)

The first sample seeds `y = x`, so the output is valid from the first
step and shows no startup transient toward zero. Compared with
`ubx/movavg` it needs O(1) state per channel and gives an infinite,
exponentially decaying memory instead of a hard cutoff.

The numeric type is configured at runtime via the `type` config; input
and output share that type, so the block is a drop-in signal filter.
Filtering is performed internally in `double`; for integer output types
the result is rounded to nearest.

## Configuration

| config     | type     | description                                      |
|------------|----------|--------------------------------------------------|
| `type`     | `char`   | ubx numeric type name of the signal (mandatory)  |
| `alpha`    | `double` | smoothing factor, 0 < alpha <= 1 (mandatory)     |
| `data_len` | `long`   | vector length; averaged per element (default 1)  |
| `loglevel` | `int`    | optional log level                               |

Smaller `alpha` smooths more; `alpha = 1` is a pass-through. As a rule
of thumb an EWMA with `alpha = 2/(N+1)` has a comparable lag to a
moving average of window N.

With `data_len` > 1 the ports are vectors and an independent average is
maintained per element (channel).

Supported `type` values are `int32_t`, `uint32_t`, `int64_t`,
`uint64_t`, `float` and `double`.

## Ports

Both ports are created at runtime with the configured `type`.

| port  | direction | type     | description       |
|-------|-----------|----------|-------------------|
| `in`  | in        | `<type>` | input signal      |
| `out` | out       | `<type>` | smoothed output   |

## Behaviour

- **NODATA** on `in`: the step is skipped and no output is produced.
- The filter state persists **across stop/start** and is only cleared
  on (re-)init.
