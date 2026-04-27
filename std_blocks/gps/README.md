# ubx/gps

GPS block that reads position, velocity, and fix quality from
[gpsd](https://gpsd.gitlab.io/gpsd/) via its shared memory interface.

## Dependencies

- `libgps-dev` (part of the gpsd package)
- `gpsd` running with an attached receiver

```sh
apt install gpsd libgps-dev
gpsd /dev/ttyACM0 -F /var/run/gpsd.sock
```

## Output port

One out-port `gps` of type `struct ubx_gps_data`:

| field               | type     | description |
|---------------------|----------|-------------|
| `time`              | `int64_t` | UTC, nanoseconds since Unix epoch |
| `latitude`          | `double` | degrees north, WGS84 |
| `longitude`         | `double` | degrees east, WGS84 |
| `altMSL`            | `double` | altitude above MSL, meters |
| `altHAE`            | `double` | altitude above WGS84 ellipsoid, meters |
| `speed`             | `double` | speed over ground, m/s |
| `track`             | `double` | course, degrees true north |
| `climb`             | `double` | vertical speed, m/s |
| `eph`               | `double` | horizontal position uncertainty, m |
| `epv`               | `double` | vertical position uncertainty, m |
| `mode`              | `int`    | `0`=not seen, `1`=no fix, `2`=2D, `3`=3D |
| `status`            | `int`    | `0`=unknown, `1`=GPS, `2`=DGPS, `3`=RTK fixed, `4`=RTK float |
| `satellites_used`   | `int`    | satellites used in solution |
| `satellites_visible`| `int`    | satellites visible |

Fields that gpsd has not yet received are set to `NaN`. Check `mode` before
using positional fields: `mode ≥ 2` → lat/lon valid; `mode == 3` → altitude valid.

## Step behaviour

Each step calls `gps_waiting()` with a 0-ms timeout (non-blocking). If gpsd has
written new data to shared memory since the last read, it calls `gps_read()` and
emits on the `gps` port. If no new data is available the port is not written and
the downstream lfrb retains its last value.

## Configuration

| config     | type  | description   |
|------------|-------|---------------|
| `loglevel` | `int` | optional log level |

## Usage

```sh
ubx-launch -c gps.usc -dbus
ubx-dbus --read=gps:gps
ubx-dbus --read-mon=gps:gps
```

See `gps.usc` for the complete example.
