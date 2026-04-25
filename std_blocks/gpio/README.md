# ubx/gpio

Generic Linux GPIO block using libgpiod v2.

## Dependencies

- `libgpiod >= 2.0` (`libgpiod-dev`, `libgpiod3`)

## Configuration

| field      | type                | description                                                           |
|------------|---------------------|-----------------------------------------------------------------------|
| `gpios`    | `ubx_gpio_config[]` | array of GPIO line configs (min 1)                                    |
| `trigee`   | `ubx_triggee`       | optional: block to trigger between reading inputs and writing outputs |
| `loglevel` | `int`               | optional log level                                                    |

### `ubx_gpio_config` fields

| field            | values                                             | description                              |
|------------------|----------------------------------------------------|------------------------------------------|
| `name`           | string                                             | GPIO line name as reported by `gpioinfo` |
| `dir`            | `0`=in, `1`=out                                    | direction                                |
| `active_low`     | `0`=active-high (default), `1`=active-low          | polarity                                 |
| `pull`           | `0`=disabled (default), `1`=pull-up, `2`=pull-down | bias                                     |
| `emit_on_change` | `0`=every step (default), `1`=only on value change | for input GPIOs                          |

## Ports

Ports are created dynamically at init time from the `gpios` config:

- input GPIO (`dir=0`) → ubx **out-port** named after the line
- output GPIO (`dir=1`) → ubx **in-port** named after the line

Port type: `unsigned int` (0 or 1).

## Usage

See `gpio.usc` for a complete example. After `ubx-launch -c gpio.usc -dbus`:

```sh
# read an input port once
ubx-dbus --read=gpio:BUTTON0

# continuously monitor an input port
ubx-dbus --read-mon=gpio:LED_EN

# drive an output GPIO
ubx-dbus --write=gpio:LED_OUT:1
ubx-dbus --write=gpio:LED_OUT:0
```
