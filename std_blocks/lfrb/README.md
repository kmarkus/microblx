# ubx/lfrb

Hard real-time, lock-free ring buffer i-block, the default for connections. Built on two lock-free queues (free + used). When full, a write drops the oldest unread element and increments `overruns`.

## Configuration

| field               | type       | description                                          |
|---------------------|------------|------------------------------------------------------|
| `type_name`         | `char`     | ubx type to transport (required)                     |
| `data_len`          | `uint32_t` | array length per element (default: 1)                |
| `buffer_len`        | `uint32_t` | number of elements in the ring (default: 1)          |
| `allow_partial`     | `int`      | accept writes shorter than data_len (default: 0)     |
| `loglevel_overruns` | `int`      | log level for overrun messages; -1 disables (default: NOTICE) |

## Ports

| port       | direction | type            | description                       |
|------------|-----------|-----------------|-----------------------------------|
| `overruns` | out       | `unsigned long` | cumulative overrun count (on change) |
