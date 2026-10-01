# webgraph

Luablock serving a live React Flow + ELK.js graph of the running node
(blocks, states, configs, ports, connections) on
<http://localhost:8888>. The page refreshes every 3 s.

**Development tool**: it listens on all interfaces without
authentication and allows any origin (CORS `*`). Don't run it on
production systems or untrusted networks.

## Dependencies

- `lua-socket`, `json.lua` (`apt install lua-socket lua-json`)
- JS libs fetched from a CDN by the browser (`@xyflow/react`, `elkjs`)

## Usage

```sh
ubx-launch --webgraph -c app.usc          # port 8888
ubx-launch --webgraph=9090 -c app.usc
ubx-launch -c app.usc,/usr/local/share/ubx/examples/usc/webgraph.usc   # as usc mixin
```

As a block (self-triggering every 50 ms, `port` config defaults to 8888):

```lua
{ name="webgraph", type="luablock:webgraph" },
{ name="webgraph", config = { thread=1, period=50 } },
```
