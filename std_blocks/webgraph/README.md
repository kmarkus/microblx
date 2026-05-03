# webgraph

A self-contained luablock that serves an interactive React Flow + ELK.js
graph of the current microblx node.

## Dependencies

- `luasocket` (runtime)
- `json.lua` (runtime, ships with ubx)
- JS libs fetched from CDN on first browser load (`@xyflow/react`, `elkjs`)

## Usage

Instantiate as a luablock and make it self-triggering:

```lua
local lbutil = require("ubx/luablock-util")

local wg = lbutil.create(nd,
  "/path/to/webgraph.lua",
  "webgraph",
  "active",
  { period = 50 }   -- poll every 50 ms
)
```

Or configure the port (default 8888):

```lua
ubx.set_config_str(wg, "port", "9090")
```

Then open `http://localhost:8888` in a browser.  The page auto-refreshes
every 3 seconds and re-runs the ELK layered layout.

## What is shown

| Element | Details |
|---------|---------|
| **cblock node** | name, prototype, state (colour-coded), configs, ports |
| **port handle** | name, type[len] shown on hover |
| **edge** | iblock name, type, data_len, buffer_len |

## Extending

- Add new routes in the `handle_request` function.
- Enrich `build_graph` to include trigger chains or node metrics.
- Adjust `ELK_OPTS` in the JS frontend for different layout algorithms.
