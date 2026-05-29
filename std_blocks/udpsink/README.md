# udpsink

Stream a block's input ports as a single JSON object per step over UDP.
Primarily intended for live-plotting with
[PlotJuggler](https://plotjuggler.io) (use the `UDP Server` streaming
input on the configured port).

`udpsink` is **not** a custom block type — it is an ordinary
[`ubx/luablock`](../luablock/README.md). The file `udpsink.lua` defines
the standard lifecycle hooks (`init`/`start`/`step`/`stop`/`cleanup`)
and is loaded with the `luablock:udpsink` convenience syntax, which
resolves `udpsink.lua` from the blocks search path. The port set and
destination are passed through the luablock's `lua_str` config:

```lua
{ name="sink", type="luablock:udpsink" },
-- ...
{ name="sink", config = {
     lua_str = [[
        ports = { ramp="double", sin="double", cos="double" }
        host  = "127.0.0.1"
        port  = 9870
     ]],
} },
```

No struct type registration and no block-prototype registration are
needed: it is a plain Lua block using mechanisms microblx already
provides.

## Dependencies

- a JSON encoder: `cjson` (preferred) or `json.lua`, providing
  `json.encode`

UDP sending uses the LuaJIT FFI directly (`socket`/`sendto`), so no
`luasocket` dependency is required. Only the socket calls use FFI and
all use fixed Linux ABI types (int fds, `size_t`/`ssize_t`, a 16-byte
`sockaddr_in`) that are identical on 32- and 64-bit ARM and x86.
Timestamps use `ubx.gettime()` rather than a raw `clock_gettime` FFI
(whose `struct timespec` is not portable across 32-bit `time_t`).

## Configuration (via `lua_str`)

| global  | type     | description                                   |
|---------|----------|-----------------------------------------------|
| `ports` | table    | required: `{ name = "type", ... }` (see below)|
| `host`  | string   | target host, default `127.0.0.1`              |
| `port`  | number   | target UDP port, default `9870` (PlotJuggler) |

`ports` maps each output JSON key / input-port name to a registered ubx
type name (scalar, length 1), e.g.
`ports = { x="double", n="int32_t" }`.

## Ports

The luablock has no static data ports. One input port is created per
entry of `ports` in the `init` hook. Because `init` runs during
`configure_blocks` (before `connect_blocks`), the ports exist by the
time the USC connections are wired up — no special lifecycle hook is
needed.

## Lifecycle

| hook      | what it does                                                   |
|-----------|----------------------------------------------------------------|
| `init`    | validate `ports`; `inport_add` one inport per entry            |
| `start`   | read `host`/`port`; open UDP socket; cache ports + read buffers |
| `step`    | drain buffers; emit one aligned JSON frame per sample; UDP send |
| `stop`    | close UDP socket                                               |
| `cleanup` | reset per-instance state                                       |

## Behaviour

Each step drains **all** buffered samples, emitting one JSON datagram
per *frame* (one sample per port); nothing is sent if no port had data.
Draining everything (not just the latest sample) is what lets the sink
run slower than, and decoupled from, its producers — see *Decoupling*
below.

**Frame alignment.** The per-port buffers are filled non-atomically (the
producers write one port after another), so a decoupled sink can catch a
cycle half-written; reading each port independently would emit a partial
frame and skew that port permanently. Instead each sample is parked in a
per-port *pending* slot and a frame emitted only once every port has one.
An incomplete frame is held for one sink step so the lagging port can
catch up; if the sample still hasn't arrived it is sent **without** it —
so a switched-off input is dropped gracefully (rejoining when it resumes)
rather than stalling the stream. This assumes inputs are normally sampled
together; ports at genuinely different rates show the slower ones as
intermittently absent.

**Timestamp.** Each datagram carries a `ts` field: a connected `ts`
input port's value if you declare one, else a monotonic `ubx.gettime()`
reading. When decoupled the auto `ts` is the *sink's* read time
(near-identical for all frames in a step), so connect a `ts` port from
the producer side for a correct per-sample axis.

## Decoupling the RT and NRT sides

Socket sends are slow, jittery syscalls you don't want on an RT trigger
chain. So keep the sink **out** of the RT trigger and run it on the
luablock's own thread at a lower rate, with deep buffers absorbing the
difference:

- self-trigger via the stock luablock configs `thread = 1` and
  `period = <msec>` (e.g. `100` for 10 Hz);
- size each connection to hold at least `producer_rate / sink_rate`
  samples plus margin, via the USC connection
  `config = { buffer_len = N }` (default 1).

Each sink step drains the whole buffer, so full-rate data still reaches
the receiver off the RT path. If the sink falls further behind than the
buffer depth the oldest samples are overwritten (best-effort, overrun
counter bumped) — the RT side never blocks.

The emitted JSON is a flat object followed by a newline, e.g.:

```json
{"ts": 12345.678, "sin": 0.408, "cos": 0.913, "tan": 0.447}
```

In PlotJuggler, enable **"use field as timestamp"** and select `ts`;
otherwise PlotJuggler stamps samples with their (jittery) arrival time,
which makes fast signals look kinked/discontinuous.

## Usage

See [`examples/usc/udpsink.usc`](../../examples/usc/udpsink.usc) for a
complete example that streams `sin`, `cos`, `tan` (with the ramp as the
`ts` axis) to PlotJuggler. It demonstrates the decoupled pattern: the
producers run at 100 Hz on a `ptrig`, while the sink self-triggers at
10 Hz (`thread=1`, `period=100`) and the connections use
`buffer_len=16`.
