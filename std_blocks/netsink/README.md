# netsink

Stream a block's input ports as one message per step over the network,
for live-plotting and telemetry. Two transports (**UDP** datagrams or a
**ZeroMQ** PUB socket) and two encodings (**JSON** or **MessagePack**)
are selectable. It is primarily intended for live-plotting with
[PlotJuggler](https://plotjuggler.io) (JSON over its `UDP Server` input).

`netsink` is **not** a custom block type — it is an ordinary
[`ubx/luablock`](../luablock/README.md). The file `netsink.lua` defines
the standard lifecycle hooks and is loaded with the `luablock:netsink`
convenience syntax. All configuration — the port set and the network
settings — is passed as globals through the luablock's `lua_str` config:

```lua
{ name="sink", type="luablock:netsink" },
-- ...
{ name="sink", config = {
     lua_str = [[
        ports     = { ts="double", sin="double", cos="double" }
        transport = "zmq"              -- "udp" (default) | "zmq"
        format    = "msgpack"          -- "json" (default) | "msgpack"
        uri       = "tcp://*:9870"     -- zmq PUB endpoint
     ]],
} },
```

No struct type registration and no block-prototype registration are
needed: it is a plain Lua block using mechanisms microblx already
provides.

## Dependencies

Only the dependency for the transport/format you actually use is needed:

- `format="json"`: a JSON encoder — `cjson` (preferred) or `json.lua`
- `format="msgpack"`: [`lua-MessagePack`](https://framagit.org/fperrad/lua-MessagePack)
  (pure Lua; `cmsgpack` also works)
- `transport="zmq"`: `libzmq` (loaded lazily via the LuaJIT FFI)

UDP and ZeroMQ are driven through the LuaJIT FFI directly, so no
`luasocket`/`lzmq` binding is required. Only the socket calls use FFI and
all use fixed, stable C ABI types. Timestamps use `ubx.gettime()` rather
than a raw `clock_gettime` FFI (whose `struct timespec` is not portable
across 32-bit `time_t`).

## Configuration (via `lua_str`)

All settings are `lua_str` globals: `ports` is required, the rest are
optional and default as shown.

| global      | type     | description                                     |
|-------------|----------|-------------------------------------------------|
| `ports`     | table    | required: `{ name = "type", ... }` (see below)  |
| `transport` | string   | `"udp"` (default) or `"zmq"`                     |
| `format`    | string   | `"json"` (default) or `"msgpack"`               |
| `host`      | string   | udp: target host, default `127.0.0.1`           |
| `port`      | number   | udp: target port, default `9870` (PlotJuggler)  |
| `uri`       | string   | zmq: PUB endpoint, default `tcp://*:9870`        |
| `zmq_bind`  | number   | zmq: `1`=bind (default), `0`=connect            |

`ports` maps each output key / input-port name to a registered ubx type
name (scalar, length 1), e.g. `ports = { x="double", n="int32_t" }`.

## Ports

The luablock has no static data ports. One input port is created per
entry of `ports` in the `preinit` hook. Because `preinit` runs during
`configure_blocks` (before `connect_blocks`), the ports exist by the
time the USC connections are wired up.

`ports` must be known in `preinit` to build the interface, which is why
all configuration is carried as `lua_str` globals (applied when the
`lua_str` chunk runs, before `preinit`) rather than as separate block
configs.

## Transports

- **udp** — one UDP datagram per frame to `host:port`. Connectionless,
  best-effort; ideal for PlotJuggler's UDP JSON input.
- **zmq** — a ZeroMQ `PUB` socket, one message per frame. By default it
  **binds** `uri` (subscribers connect and `SUBSCRIBE ""`); set
  `zmq_bind=0` to connect instead. Sends are non-blocking (`ZMQ_DONTWAIT`):
  with no/slow subscriber, ZeroMQ drops the message rather than blocking
  the sink — note the usual PUB *slow-joiner* caveat (messages published
  before a subscriber has connected are lost).

## Encodings

- **json** — a flat object followed by a newline, e.g.
  `{"ts": 12345.678, "sin": 0.408}\n`. The newline delimits messages for
  stream consumers; PlotJuggler's UDP JSON parser is happy with it.
- **msgpack** — the same flat map, MessagePack-encoded. Frames are
  self-delimiting, so no separator is added; one datagram / ZeroMQ
  message is exactly one map.

## Lifecycle

| hook      | what it does                                                       |
|-----------|-------------------------------------------------------------------|
| `preinit` | validate `ports`; `inport_add` one inport per entry            |
| `start`   | select encoder + transport; open socket/ZeroMQ; cache read buffers |
| `step`    | drain buffers; emit one aligned frame per sample; send            |
| `stop`    | close the transport                                               |
| `cleanup` | reset per-instance state                                          |

## Behaviour

Each step drains **all** buffered samples, emitting one message per
*frame* (one sample per port); nothing is sent if no port had data.
Draining everything (not just the latest sample) is what lets the sink
run slower than, and decoupled from, its producers.

**Frame alignment.** The per-port buffers are filled non-atomically, so a
decoupled sink can catch a cycle half-written; reading each port
independently would emit a partial frame and skew that port permanently.
Instead each sample is parked in a per-port *pending* slot and a frame
emitted only once every port has one. An incomplete frame is held for one
sink step so the lagging port can catch up; if the sample still hasn't
arrived it is sent **without** it — so a switched-off input is dropped
gracefully (rejoining when it resumes) rather than stalling the stream.

**Timestamp.** Each frame carries a `ts` field: a connected `ts` input
port's value if you declare one, else a monotonic `ubx.gettime()`
reading. When decoupled the auto `ts` is the *sink's* read time
(near-identical for all frames in a step), so connect a `ts` port from
the producer side for a correct per-sample axis.

## Decoupling the RT and NRT sides

Socket/ZeroMQ sends are slow, jittery syscalls you don't want on an RT
trigger chain. So keep the sink **out** of the RT trigger and run it on
the luablock's own thread at a lower rate, with deep buffers absorbing
the difference:

- self-trigger via the stock luablock configs `thread = 1` and
  `period = <msec>` (e.g. `100` for 10 Hz);
- size each connection to hold at least `producer_rate / sink_rate`
  samples plus margin, via the USC connection
  `config = { buffer_len = N }` (default 1).

Each sink step drains the whole buffer, so full-rate data still reaches
the receiver off the RT path. If the sink falls further behind than the
buffer depth the oldest samples are overwritten (best-effort, overrun
counter bumped) — the RT side never blocks.

In PlotJuggler, enable **"use field as timestamp"** and select `ts`;
otherwise PlotJuggler stamps samples with their (jittery) arrival time,
which makes fast signals look kinked/discontinuous.

## Usage

See [`examples/usc/netsink.usc`](../../examples/usc/netsink.usc) for a
complete example that streams `sin`, `cos`, `tan` (with the ramp as the
`ts` axis). It demonstrates the decoupled pattern: producers run at
100 Hz on a `ptrig` while the sink self-triggers at 10 Hz
(`thread=1`, `period=100`) with `buffer_len=16` connections.

Quick UDP/JSON check without PlotJuggler:

```sh
nc -u -l 9870
```
