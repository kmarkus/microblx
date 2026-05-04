# ubx/lfds_cyclic

Hard real-time, lock-free cyclic (overwriting) ring buffer i-block based on liblfds611. On overflow the oldest element is overwritten and the `overruns` port is updated.

## Dependencies

- `liblfds611`

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
