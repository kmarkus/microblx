# luablock

Generic LuaJIT-based c-block. Implement block behavior in a Lua file or string by defining any subset of the standard lifecycle hooks.

## Configuration

| field       | type   | description                                          |
|-------------|--------|------------------------------------------------------|
| `lua_file`  | `char` | path to a Lua file to load                           |
| `lua_str`   | `char` | inline Lua source (alternative to lua_file)          |
| `thread`    | `int`  | if 1, spawn a self-triggering thread (default: 0)    |
| `period`    | `int`  | thread period in milliseconds (required if thread=1) |
| `loglevel`  | `int`  | optional log level                                   |

## Ports

| port       | direction | type   | description                                      |
|------------|-----------|--------|--------------------------------------------------|
| `exec_str` | in/out    | `char` | write Lua code here to execute it at runtime     |

## Lua hooks

Define any of these global functions in the Lua file:

```lua
function init(block)    return true end
function start(block)   return true end
function step(block)    end
function stop(block)    end
function cleanup(block) end
```

The `block` argument is the raw `ubx_block_t*`; cast with `ffi.cast("ubx_block_t*", block)` for full API access.

## Usage

```lua
-- in a USC file
{ name="foo", type="luablock:foo" }
-- config
{ name="foo", config={ lua_file="/path/to/foo.lua" } }
```

See `luablock-example.lua` for a minimal example and `luablock-util.lua` for the higher-level `lbutil.create()` helper.
