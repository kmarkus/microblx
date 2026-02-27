# lsdb-intf: D-Bus interface block



## LoadUSC Limitations

The `LoadUSCLua` and `LoadUSCJSON` D-Bus methods accept a USC model
as a string. Since the model is not loaded from a file, the following
limitations apply compared to file-based `.usc` loading:

- **No file-based subsystem loading**: `bd.load("file.usc")` in
  `subsystems` requires a file path. Relative paths will resolve
  against the current working directory of the node process, not
  against any source file.
- **No relative file paths**: any file reference (e.g. `lua_file` in
  a luablock configuration) must use an absolute path or the
  `luablock:name` syntax, which searches the standard installation
  prefixes.
- **JSON-specific**: the JSON format cannot express Lua constructs
  such as computed values, variables or function calls. All values
  must be JSON literals. Subsystem loading via `bd.load()` is
  unavailable in JSON USC models.


## Examples

### Start it up

```sh
$ ubx-launch -c /usr/local/share/ubx/examples/usc/threshold.usc -v -dbus -s -l 8
```

### Show system state

```sh
 $ ubx-dbus -i
node:         n
modules:      {
  {"/usr/local/lib/ubx/0.9/stdtypes.so", "BSD-3-Clause"},
  {"/usr/local/lib/ubx/0.9/ptrig.so", "BSD-3-Clause"},
  {"/usr/local/lib/ubx/0.9/lfrb.so", "BSD-3-Clause"},
  {"/usr/local/lib/ubx/0.9/mqueue.so", "BSD-3-Clause"},
  {"/usr/local/lib/ubx/0.9/threshold.so", "BSD-3-Clause"},
  {"/usr/local/lib/ubx/0.9/ramp_double.so", "BSD-3-Clause"},
  {"/usr/local/lib/ubx/0.9/math_double.so", "BSD-3-Clause"},
  {"/usr/local/lib/ubx/0.9/luablock.so", "BSD-3-Clause"},
}
cblock types: {"ubx/ptrig", "ubx/threshold", "ubx/ramp_double", "ubx/math_double", "ubx/luablock"}
iblock types: {"ubx/lfrb", "ubx/mqueue"}
cblocks:      {
  {"thres", "ubx/threshold", "active"},
  {"ramp", "ubx/ramp_double", "active"},
  {"sin", "ubx/math_double", "active"},
  {"trigger", "ubx/ptrig", "inactive"},
  {"lsdb0", "ubx/luablock", "active"},
  {"lsdb0-ptrig", "ubx/ptrig", "active"},
}
connections:  {
  {from="i_00000002", to={"thres", "in"}},
  {from={"thres", "event"}, to="i_00000003"},
  {from={"ramp", "out"}, to="i_00000001"},
  {from="i_00000001", to={"sin", "x"}},
  {from={"sin", "y"}, to="i_00000002"},
}
```

### Show a block

```sh
$ ubx-dbus -i thres
{
  attrs={},
  block_type="cblock",
  configs={
    {doc="", name="threshold", type_name="double", value=0.8},
    {doc="", name="loglevel", type_name="int", value=8},
  },
  meta_data="",
  name="thres",
  ports={
    { attrs=1,
      connections={incoming={"i_00000002"}, outgoing={}},
      doc="",
      in_data_len=1,
      in_type_name="double",
      name="in",
    },
    {
      attrs=1,
      connections={incoming={}, outgoing={}},
      doc="",
      name="state",
      out_data_len=1,
      out_type_name="int",
    },
    {
      attrs=1,
      connections={incoming={}, outgoing={"i_00000003"}},
      doc="",
      name="event",
      out_data_len=1,
      out_type_name="struct thres_event",
    },
  },
  prototype="ubx/threshold",
  stat_num_steps=5777,
  state="active",
}

```

### Change block state

```sh
# reconfigure
$ ubx-dbus -s trigger:preinit
$ ubx-dbus -s trigger:active
```

### Reconfiguring a trigger

```sh
$ ubx-dbus -s trigger:inactive
$ ubx-dbus -c trigger:chain0
{
  {b="ramp", every=1, num_steps=1},
  {b="sin", every=1, num_steps=1},
  {b="thres", every=1, num_steps=1},
}

# reconfigure with every=2
$ $ ubx-dbus -c trigger:chain0:'{
  {b="#ramp", every=2, num_steps=1}, 
  {b="#sin", every=2, num_steps=1}, 
  {b="#thres", every=2, num_steps=1}, 
}'
$ ubx-dbus -s trigger:active
```

> **Note**: the same `#BLOCK` syntax as in `.usc` files is used to
> indicate blocks.

### Manually trigger a chain

```sh
# stop the trigger
$ ubx-dbus -s trigger:inactive
# trigger the chain manually
$ ubx-dbus -t ramp:sin:thres
```

### Read from a port

```sh
$ ubx-dbus -r sin:y
0.99698063694881

$ ubx-dbus -r thres:event
{dir=1, ts={nsec=507728369, sec=70768}}
```

> **Note**: when no data is available, `false` is returned. It is
> quite likely that this happens upon the first read.

using `-R` (`--read-mon`) the port can be read continuously:

```sh
$ ubx-dbus -R thres:event
{dir=0, ts={nsec=166774413, sec=70836}}
{dir=1, ts={nsec=284097045, sec=70836}}
{dir=0, ts={nsec=314593892, sec=70836}}
{dir=1, ts={nsec=425048420, sec=70836}}
{dir=0, ts={nsec=451531259, sec=70836}}
```

> **Note**: reading this way is very inefficent and really only useful
> for for very slow signals or debugging. A much more efficient
> approach is using the `mqueue` iblock or a shared memory buffer.

### write to a port

```sh
$ ubx-dbus -s trigger:inactive

# monitor the threshold output in another terminal using
#   $ ubx-mq read thres.event -p threshold

# rising (dir=1)
$ ubx-dbus -w thres:in:1 && ubx-dbus -t thres

# falling (dir=0)
$ ubx-dbus -w thres:in:0 && ubx-dbus -t thres
```

### connect a blocks

