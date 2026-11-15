# webgraph

Luablock serving a live React Flow + ELK.js graph of the running node
(blocks, states, configs, ports, connections) on
<http://localhost:8888>. The page refreshes every 3 s.

> **Note**: development tool only. It listens on all interfaces
> without authentication and allows any origin (CORS `*`). Don't run it
> on production systems or untrusted networks.

An iblock connecting two blocks (e.g. `lfrb`, `vstore`, `latch`) is
shown as an edge, labelled with the iblock name and type, the data type
and the read/write counts, e.g. `i_00000003 [vstore]` / `double r:12 w:12`.
iblocks with only one side connected (e.g. `mqueue`) are shown as nodes.

A `ubx/pack` and a `ubx/unpack` block of the same segment are joined by
one fat edge, labelled with the segment, as all ports of a frame pass
through it.

## Dependencies

- `lua-socket`, `json.lua` (`apt install lua-socket lua-json`)
- JS libs fetched from a CDN by the browser (`@xyflow/react`, `elkjs`)

## Usage

```sh
ubx-launch --webgraph -c app.usc          # port 8888
ubx-launch --webgraph=9090 -c app.usc
ubx-launch -c app.usc,/usr/local/share/ubx/examples/usc/webgraph.usc   # as usc mixin
ubx-dbus --webgraph=9090                  # into a running node (--dbus)
```

As a block (self-triggering every 50 ms, `port` config defaults to 8888):

```lua
{ name="webgraph", type="luablock:webgraph" },
{ name="webgraph", config = { thread=1, period=50 } },
```
