# ubx/cconst, ubx/iconst

Constant value blocks. `ubx/cconst` is a c-block that writes a fixed value to its output port each step. `ubx/iconst` is a read-only i-block that returns the same value on every read.

The type is generic: set `type_name` to any registered ubx type.

## Configuration

| field       | type     | description                                      |
|-------------|----------|--------------------------------------------------|
| `type_name` | `char`   | name of the ubx type to hold (required)          |
| `data_len`  | `long`   | array length (default: 1)                        |
| `value`     | dynamic  | the constant value; added at init from type_name |

## Ports (cconst only)

| port  | direction | type      | description    |
|-------|-----------|-----------|----------------|
| `out` | out       | *dynamic* | constant value |
