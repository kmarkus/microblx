# ubx/lfrb

Hard real-time, lock-free ring buffer i-block. Uses two lock-free queues (free + used) so writes never overwrite unread data — instead the oldest is dropped and `overruns` is incremented.

Preferred over `ubx/lfds_cyclic` for new designs; no external library dependency.

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
