# lsdb-intf: D-Bus interface block

Exposes a ubx node over D-Bus using the lsdbus Lua bindings.

## Examples

### Start

```sh
$ ubx-launch -c /usr/local/share/ubx/examples/usc/threshold.usc -v -dbus -s -l 8
```

### Show system state

```sh
$ ubx-dbus -i
node:         n
modules:      { {"/usr/local/lib/ubx/0.9/stdtypes.so", "BSD-3-Clause"}, ... }
cblock types: {"ubx/ptrig", "ubx/threshold", "ubx/ramp_double", "ubx/math_double", "ubx/luablock"}
iblock types: {"ubx/lfrb", "ubx/mqueue"}
cblocks:      {
  {"thres", "ubx/threshold", "active"},
  {"ramp", "ubx/ramp_double", "active"},
  {"sin", "ubx/math_double", "active"},
  {"trigger", "ubx/ptrig", "inactive"},
  {"lsdb0", "ubx/luablock", "active"},
}
connections:  {
  {from={"ramp", "out"}, to="i_00000001"},
  {from="i_00000001", to={"sin", "x"}},
  ...
}
```

### Show a block

```sh
$ ubx-dbus -i thres
{
  name="thres", prototype="ubx/threshold", state="active",
  configs={
    {name="threshold", type_name="double", value=0.8},
    {name="loglevel",  type_name="int",    value=8},
  },
  ports={
    {name="in",    in_type_name="double",              connections={incoming={"i_00000002"}}},
    {name="state", out_type_name="int"},
    {name="event", out_type_name="struct thres_event", connections={outgoing={"i_00000003"}}},
  },
  stat_num_steps=5777,
}
```

### Change block state

```sh
$ ubx-dbus -s trigger:preinit
$ ubx-dbus -s trigger:active
```

### Reconfigure a trigger

```sh
$ ubx-dbus -s trigger:inactive
$ ubx-dbus -c trigger:chain0
{
  {b="ramp", every=1, num_steps=1},
  {b="sin", every=1, num_steps=1},
  {b="thres", every=1, num_steps=1},
}

# reconfigure with every=2
$ ubx-dbus -c trigger:chain0:'{
  {b="#ramp", every=2, num_steps=1},
  {b="#sin", every=2, num_steps=1},
  {b="#thres", every=2, num_steps=1},
}'
$ ubx-dbus -s trigger:active
```

> **Note**: the same `#BLOCK` syntax as in `.usc` files is used to reference blocks.

### Manually trigger a chain

```sh
$ ubx-dbus -s trigger:inactive
$ ubx-dbus -t ramp:sin:thres
```

### Read from a port

```sh
$ ubx-dbus -r sin:y
0.99698063694881

$ ubx-dbus -r thres:event
{dir=1, ts={nsec=507728369, sec=70768}}
```

> **Note**: `false` is returned when no data is available.

Use `-R` (`--read-mon`) to read continuously:

```sh
$ ubx-dbus -R thres:event
{dir=0, ts={nsec=166774413, sec=70836}}
{dir=1, ts={nsec=284097045, sec=70836}}
...
```

> **Note**: continuous reads are inefficient; prefer `mqueue` or shared memory for real use.

### Write to a port

```sh
$ ubx-dbus -s trigger:inactive

# monitor threshold output: ubx-mq read thres.event -p threshold

# rising (dir=1)
$ ubx-dbus -w thres:in:1 && ubx-dbus -t thres

# falling (dir=0)
$ ubx-dbus -w thres:in:0 && ubx-dbus -t thres
```

### Clear a node

```sh
$ ubx-dbus -C                 # clear all blocks (lsdb-intf itself is always kept)
$ ubx-dbus -C=ptrig0:^logger  # keep ptrig0 and any block matching ^logger
```

Keeplist entries: plain strings match exactly; entries starting with `^` or ending with `$`
are Lua [string.match](https://www.lua.org/manual/5.1/manual.html#pdf-string.match) patterns.

### Connect blocks

```sh
$ ubx-dbus -C srcblock:srcport:tgtblock:tgtport
```

## Plugins

User-specific D-Bus interfaces can be added to a running node by loading *plugins*. Each
plugin registers one D-Bus object on its own path and interface, independent of `org.ubx.node`.

### Plugin file format

```lua
-- my_robot_plugin.lua
local M = {}

function M.init(ctx)
   -- ctx.nd    – ubx_node_t*
   -- ctx.bus   – lsdbus bus connection
   -- ctx.api.* – pre-bound wrappers: create_block, remove_block, switch_state, trigger,
   --             connect, set_config, get_config, write, read,
   --             load_module, load_usc_lua, load_usc_json, clear_node

   return {
      path = "/robot",
      intf = {
         name = "org.myapp.robot",
         methods = {
            StartMission = {
               { direction='in', name='mode', type='s' },
               handler = function(vt, mode)
                  ctx.api.switch_state("mission_ctrl", "active")
               end,
            },
         },
      },
   }
end

function M.cleanup() end   -- optional; called before unload

return M
```

`init(ctx)` must return a table with:
- `path` (`string`): D-Bus object path (e.g. `"/robot"`)
- `intf` (`table`): lsdbus interface definition (`name`, `methods`, `properties`, `signals`)

### Plugin search path

- **Absolute path** (starts with `/`): loaded directly.
- **Short name**: resolved from `<prefix>/share/ubx/lsdb-intf.d/`. The `.lua` suffix is
  optional; the plugin is listed and unloaded by the base name without extension.

```sh
$ ubx-dbus call --interface org.ubx.pluginmanager LoadPlugin s:"/opt/myapp/robot_plugin.lua"
$ ubx-dbus call --interface org.ubx.pluginmanager LoadPlugin s:"robot_plugin"
$ ubx-dbus call --interface org.ubx.pluginmanager LoadPlugin s:"robot_plugin.lua"  # also accepted
```

### Loading at startup

```lua
{ name="lsdb0", config = { thread=1, period=100,
                            plugins="robot_plugin;sensors_plugin" } }
```

### Managing at runtime

Plugin management is on the `org.ubx.pluginmanager` interface at object path `/`:

```sh
$ ubx-dbus call --interface org.ubx.pluginmanager LoadPlugin   s:"robot_plugin"
$ ubx-dbus call --interface org.ubx.pluginmanager ListPlugins
$ ubx-dbus call --interface org.ubx.pluginmanager UnloadPlugin s:"robot_plugin"
```

All plugins are automatically unloaded when the block stops.


## LoadUSC Limitations

`LoadUSCLua` and `LoadUSCJSON` accept a USC model as a string. Compared to file-based loading:

- **No file-based subsystem loading**: `bd.load("file.usc")` in `subsystems` requires a file
  path; relative paths resolve against the node process's working directory.
- **No relative file paths**: file references (e.g. `lua_file`) must use absolute paths or
  the `luablock:name` syntax.
- **JSON-specific**: JSON cannot express Lua constructs (computed values, variables, function
  calls); `bd.load()` is unavailable in JSON USC models.
