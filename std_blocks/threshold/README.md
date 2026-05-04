# ubx/threshold

Checks whether a `double` input exceeds a threshold. Outputs the current state (above/below) and emits a `struct thres_event` whenever the threshold is crossed.

## Configuration

| field       | type     | description                       |
|-------------|----------|-----------------------------------|
| `threshold` | `double` | threshold value (required)        |
| `loglevel`  | `int`    | optional log level                |

## Ports

| port    | direction | type                  | description                              |
|---------|-----------|-----------------------|------------------------------------------|
| `in`    | in        | `double`              | signal to compare                        |
| `state` | out       | `int`                 | 1 if above threshold, 0 if below         |
| `event` | out       | `struct thres_event`  | emitted on each crossing (`dir`, `ts`)   |
