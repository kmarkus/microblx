# ubx/math_double, ubx/math_float

Applies a single-argument `math.h` function element-wise to an input array. Optional per-element `mul` and `add` are applied after the function: `y = f(x) * mul + add`.

## Configuration

| field      | type     | description                                              |
|------------|----------|----------------------------------------------------------|
| `func`     | `char`   | math function name (required); see list below            |
| `data_len` | `long`   | array length (default: 1)                                |
| `mul`      | *T*      | per-element multiplier applied after f(x) (default: 1)  |
| `add`      | *T*      | per-element offset added after mul (default: 0)          |

Supported functions: `sin`, `asin`, `sinh`, `asinh`, `cos`, `acos`, `cosh`, `acosh`, `tan`, `atan`, `tanh`, `atanh`, `cbrt`, `ceil`, `erf`, `erfc`, `exp`, `exp2`, `expm1`, `fabs`, `floor`, `j0`, `j1`, `lgamma`, `log`, `log10`, `log1p`, `log2`, `logb`, `nearbyint`, `rint`, `round`, `sqrt`, `tgamma`, `trunc`, `y0`, `y1`.

## Ports

| port | direction | type | description  |
|------|-----------|------|--------------|
| `x`  | in        | *T*  | input array  |
| `y`  | out       | *T*  | output array |
