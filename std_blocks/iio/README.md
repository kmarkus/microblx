# ubx/iio and ubx/iio_buf

Linux Industrial I/O blocks using libiio for ADCs, DACs, IMUs, and
pressure/temperature/humidity sensors.

Two blocks are provided:

| Block | Mode | Use when |
|-------|------|----------|
| `ubx/iio` | **Polled** — synchronous sysfs read each step | slow sensors, DAC output, hwmon (coretemp, etc.) |
| `ubx/iio_buf` | **Buffered** — hardware samples at `sampling_frequency`; ptrig drains the buffer | high-speed ADCs, IMUs, any device where sample rate matters |

Both blocks create ports dynamically from the `channels` config. Port type is
always `double` (physical, scaled value). Physical value = `(raw + offset) * scale`.

## Dependencies

- `libiio >= 0.21` (`libiio-dev`, `libiio0`)

## Discovering devices and channels

```sh
# install IIO tools (Debian)
apt install libiio-utils

# list all IIO devices and their channels with attributes
iio_info

# compact list
iio_info -s

# read a channel attribute directly (bypass ubx, for testing)
iio_attr -c 48300000.adc voltage0 raw
iio_attr -c 48300000.adc voltage0 scale
```

The `device` config field accepts either the IIO device **name** (the label after
the colon in `iio_info` output, e.g. `"48300000.adc"`, `"bmi088"`, `"coretemp"`)
or the **ID** (e.g. `"iio:device0"`). Using the name is more stable across reboots.

## Shared channel config: `ubx_iio_channel_config`

Used by both blocks in the `channels` config array.

| field                | type     | description |
|----------------------|----------|-------------|
| `device`             | `char[]` | IIO device name or `"iio:deviceN"` ID |
| `channel`            | `char[]` | channel ID as shown by `iio_info` (e.g. `"voltage0"`, `"accel_x"`, `"temp"`) |
| `direction`          | `int`    | `0` = device-input → out-port (default), `1` = device-output → in-port (`ubx/iio` only) |
| `sampling_frequency` | `double` | Hz; `0` = use driver default. In `ubx/iio_buf` this is the hardware data rate. |
| `scale`              | `double` | `0` = read from sysfs (default); nonzero overrides sysfs scale **and** offset |
| `offset`             | `double` | used only when `scale != 0` |

### Scale/offset notes

The kernel does not apply scale/offset automatically; both blocks compute
`(raw + offset) * scale` in userspace. By default, scale and offset are read
from the driver's sysfs attributes at init. Set `scale` to override both, e.g.:

```lua
-- hwmon reports millidegrees; convert to degrees C
{ device="coretemp", channel="temp10", scale=0.001, offset=0 }

-- sensor with no sysfs scale (known sensitivity)
{ device="ads1115", channel="voltage0", scale=0.0001875 }
```

## Common channel IDs

| Device class  | Channel IDs |
|---------------|-------------|
| ADC           | `voltage0`, `voltage1`, … |
| DAC           | `voltage0`, `voltage1`, … (`direction=1`) |
| Accelerometer | `accel_x`, `accel_y`, `accel_z` |
| Gyroscope     | `anglvel_x`, `anglvel_y`, `anglvel_z` |
| Magnetometer  | `magn_x`, `magn_y`, `magn_z` |
| Temperature   | `temp` |
| Pressure      | `pressure` |
| Humidity      | `humidityrelative` |
| Illuminance   | `illuminance` |
| Proximity     | `proximity` |

---

## ubx/iio — polled block

Each step triggers a synchronous sysfs read (`raw` or `input`) per channel.
The ptrig rate is the data rate. `sampling_frequency`, if set, configures the
hardware conversion rate/bandwidth but does not change when the block emits data.

Supports both input (sensor → out-port) and output (in-port → DAC) channels.
hwmon devices (e.g. `coretemp`) are supported via the `input` sysfs attribute,
which is probed automatically at init.

### Configuration

| config     | type                       | description                    |
|------------|----------------------------|--------------------------------|
| `channels` | `ubx_iio_channel_config[]` | channel configs (≥ 1)          |
| `loglevel` | `int`                      | optional log level             |

### Ports

- `direction=0` (device-input) → ubx **out-port** named after the channel ID
- `direction=1` (device-output) → ubx **in-port** named after the channel ID

### BeagleBone Black example

```sh
iio_info | grep -A5 "\.adc"         # verify device name
ubx-launch -c iio_bbb.usc -dbus
ubx-dbus --read=iio:voltage0
ubx-dbus --read-mon=iio:voltage1
```

See `iio_bbb.usc` for the complete usc.

### Mixed ADC + DAC example

```lua
channels = {
    { device="ad7768", channel="voltage0" },           -- ADC → out-port
    { device="ad5686", channel="voltage0", direction=1 }, -- DAC → in-port
},
```

---

## ubx/iio_buf — buffered block

The device samples at the hardware rate set by `sampling_frequency`. Each ptrig
step does a **non-blocking poll** on the kernel buffer; if data is ready it
drains the buffer and emits the most recent sample on each port. If no data has
arrived yet the port is not written and the downstream lfrb retains the last
value.

Input channels only. Use `ubx/iio` for DAC output.

Channels from the **same device** share one buffer and must use the same
`sampling_frequency`; configuring different rates for channels on the same
device is an init error. Channels from **different devices** are fully
independent.

The ptrig should run at `max(all device sampling_frequencies)`. Slower devices
emit every Nth step naturally.

### Configuration

| config         | type                       | description |
|----------------|----------------------------|-------------|
| `channels`     | `ubx_iio_channel_config[]` | input channel configs (≥ 1) |
| `trigger`      | `char`                     | IIO trigger name (optional; empty = device's current trigger) |
| `buffer_len`   | `int`                      | kernel buffer depth in samples (default `4`; must be power of 2 for many drivers) |
| `timeout_ms`   | `int`                      | poll timeout per step: `0` = non-blocking (default, safe for RT), `-1` = block until data |
| `loglevel`     | `int`                      | optional log level |

### Ports

One ubx **out-port** per channel, named after the channel ID, type `double`.
Port is written only when new buffer data arrives.

### Triggers

Many IIO devices require a trigger before buffered capture works. Common options:

```sh
# list available triggers
iio_attr -a -C                       # or: ls /sys/bus/iio/trigger/

# hrtimer trigger (kernel module iio-trig-hrtimer)
modprobe iio-trig-hrtimer
# creates "hrtimer0"; set its rate via sysfs sampling_frequency
```

If the device has a built-in trigger (e.g. most IMUs), leave `trigger` empty.

### IMU example (BMI088 at 400 Hz)

```lua
return bd.system {
   imports = { "stdtypes", "ptrig", "iio_buf" },
   blocks = {
      { name="imu",  type="ubx/iio_buf" },
      { name="trig", type="ubx/ptrig" },
   },
   configurations = {
      {
         name = "imu",
         config = {
            channels = {
               { device="bmi088", channel="accel_x", sampling_frequency=400 },
               { device="bmi088", channel="accel_y", sampling_frequency=400 },
               { device="bmi088", channel="accel_z", sampling_frequency=400 },
               { device="bmi088", channel="anglvel_x", sampling_frequency=400 },
               { device="bmi088", channel="anglvel_y", sampling_frequency=400 },
               { device="bmi088", channel="anglvel_z", sampling_frequency=400 },
            },
            buffer_len = 4,
         }
      },
      {
         name = "trig",
         config = {
            period = { sec=0, usec=2500 },  -- 400 Hz
            chain0 = { { b="#imu" } },
         }
      },
   },
   connections = {},
}
```

### Multi-device example (10 kHz ADC + 5 kHz ADC)

```lua
-- ptrig runs at 10 kHz; the 5 kHz device emits every second step
channels = {
    { device="ad7768",  channel="voltage0", sampling_frequency=10000 },
    { device="ad7606c", channel="voltage0", sampling_frequency=5000 },
},
```
