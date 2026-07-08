# ubx/threshold

Checks whether a `double` input exceeds a threshold. Outputs the current state (above/below) and emits a `struct thres_event` whenever the threshold is crossed.

An optional `hysteresis` band suppresses chatter on noisy signals
(Schmitt trigger): the state switches to 1 only above
`threshold + hysteresis/2` and back to 0 only below
`threshold - hysteresis/2`; within the band the previous state is kept.
The default of 0 gives a plain comparison.

## Configuration

| field        | type     | description                                  |
|--------------|----------|----------------------------------------------|
| `threshold`  | `double` | threshold value (required)                   |
| `hysteresis` | `double` | width of the hysteresis band (default 0)     |
| `loglevel`   | `int`    | optional log level                           |

## Ports

| port    | direction | type                  | description                              |
|---------|-----------|-----------------------|------------------------------------------|
| `in`    | in        | `double`              | signal to compare                        |
| `state` | out       | `int`                 | 1 if above threshold, 0 if below         |
| `event` | out       | `struct thres_event`  | emitted on each crossing (`dir`, `ts`)   |
