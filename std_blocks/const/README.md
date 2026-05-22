# ubx/cconst, ubx/iconst

Constant value blocks. `ubx/cconst` is a c-block that writes a fixed value to its output port each step. `ubx/iconst` is a read-only i-block that returns the same value on every read.

The type is generic: set `type_name` to any registered ubx type.

The held value is initialised from the `value` config. It can be
updated at runtime by writing the new value to the `in` port. For the
c-block, `in` is read once at the beginning of `step()` before the
value is written to `out`. For the i-block, `in` is read once at the
beginning of `read()` before the value is copied to the caller's
buffer. When no new value is present on `in`, the previously held
value is kept.

## Configuration

| field       | type     | description                                      |
|-------------|----------|--------------------------------------------------|
| `type_name` | `char`   | name of the ubx type to hold (required)          |
| `data_len`  | `long`   | array length (default: 1)                        |
| `value`     | dynamic  | the constant value; added at init from type_name |

## Ports

| port  | direction | type      | available in     | description                       |
|-------|-----------|-----------|------------------|-----------------------------------|
| `out` | out       | *dynamic* | cconst           | constant value                    |
| `in`  | in        | *dynamic* | cconst, iconst   | update the held value at runtime  |
