# ubx/pid

Discrete-time PID controller. Supports array-valued signals (e.g. multi-axis control) via `data_len`. Gains default to 0 if unset.

## Configuration

| field      | type     | description                           |
|------------|----------|---------------------------------------|
| `Kp`       | `double` | proportional gain (default: 0)        |
| `Ki`       | `double` | integral gain (default: 0)            |
| `Kd`       | `double` | derivative gain (default: 0)          |
| `data_len` | `long`   | signal array length (default: 1)      |

Each gain can be a scalar or an array of length `data_len`.

## Ports

| port  | direction | type     | description      |
|-------|-----------|----------|------------------|
| `msr` | in        | `double` | measured value   |
| `des` | in        | `double` | desired value    |
| `out` | out       | `double` | controller output |
