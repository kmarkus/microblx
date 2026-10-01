Model parameters for ubx-launch
===============================

2026-10-01, mk

Status: implemented.

Goal
----

Let a `.usc` model declare named parameters and let `ubx-launch` set
them on the command line:

```
ubx-launch -c arm.usc -D GRIPPER=1 -D PERIOD=500
ubx-launch -c arm.usc --params
```

Today models read tunables with `os.getenv()`. That has two problems:

- a misspelled variable is silently ignored, the model falls back to
  its default and the run looks valid.
- the only way to learn which variables a model reads is to read its
  source.

Scanning the environment cannot fix the first one, since nothing tells
`PERIDO` apart from any unrelated variable. On the command line
`ubx-launch` knows exactly what was passed, so it can check every name
against what the model declared.

Model side
----------

```lua
local PERIOD  = bd.param("PERIOD",  1000, "trigger period [us]")
local GRIPPER = bd.param("GRIPPER", 0,    "1 = gripper attached")
```

`bd.param(name, default, help)`:

- returns the value given with `-D name=...`, else `default`.
- converts the value to the type of `default`: a number default
  rejects `PERIOD=1ms` with an error naming the parameter. Number
  and string defaults only.
- `name` must match `^[A-Za-z_][A-Za-z0-9_]*$`.
- optional 4th argument `check`: a function or callable table, e.g. a
  tableshape type, `check(value) -> ok, errmsg`. It is applied to the
  default (also outside a context) and to the converted value, not
  when the conversion fails. A non-callable `check` is an error, so a
  misplaced argument is not silently ignored. Repeated declarations
  each run their own check; checks are not part of the conflict rule,
  since functions only compare by identity.
- records `name`, `default`, `help` and the declaring file in the
  current load context (see below).

`bd` is already in the environment `load_str()` sets up for a usc, so
no new global is needed. A usc that does `require("blockdiagram")`
gets the same module and the same context.

Load context
------------

Values reach a model through a load context, not through a
`bd.load()` argument:

```lua
bd.params_begin({PERIOD="500"})      -- values from -D
local m = bd.load("a.usc")           -- nested loads inherit the context
local decls, warnings = bd.params_end()
```

`params_end()` returns the declarations as an array of `{name,
default, help, file}` in declaration order, and the warnings (see
below). `bd.with_params(values, func)` wraps begin/end and also closes
the context when `func` fails; the tools use it.

- every `bd.load()`/`bd.load_str()` between begin and end reads from
  and declares into the context, including the nested `bd.load()`
  calls a composite model uses to include subsystems. Existing
  composite models need no change.
- `bd.params_end()` checks the `-D` names against the declarations
  and raises the unknown-parameter error.
- outside a context `bd.param()` returns `default` and records
  nothing, so `load()`/`load_str()` callers that know nothing about
  parameters are unaffected and no state accumulates in long running
  processes.
- nested `params_begin()` is an error.

Rejected alternative: pass the values as a `bd.load()` argument. Every
composite model would then have to forward them to its nested
`bd.load()` calls, and a forgotten forward silently yields defaults.

Repeated declarations
---------------------

The same name may be declared by several files, e.g. a base model and
an overlay, or a submodel included twice. Default and type must then
be identical, since each file evaluates its parameters at load time:
two different defaults would end up as two different values in the
merged system, which `model:merge()` cannot detect.

- default or type differs, name not given with `-D`: error, naming
  both files:

  ```
  error: parameter PERIOD: default 500 in b.usc conflicts with 1000 in a.usc
  ```

- default or type differs, name given with `-D`: warning with the same
  text. All files read the `-D` value, so the system is consistent.
- help differs: the first help text is kept.

An overlay that wants a different value sets the configuration
directly or is launched with `-D`.

Namespace: parameter names are global across all loaded files,
including submodels. A submodel included twice reads the same value
in both instances.

ubx-launch
----------

- `-D, --define=NAME=VALUE`: repeatable; optparse already returns a
  repeated option as a table. Split on the first `=`; a missing `=` or
  an invalid name is an error. A repeated name: the last value wins,
  like for the other options.
- all `-c` files are loaded and merged inside one context, so overlays
  can declare and read parameters too.
- after loading, any `-D` name that no file declared is an error:

  ```
  error: unknown parameter PERIDO; declared: PERIOD GRIPPER
  ```

- `-D` or `--params` together with `-e` is an error.
- warnings are printed to stderr; with `--werror` they are an error.
- `--params`: load the models, print name, default and help per
  declared parameter, exit 0.

ubx-dbus
--------

`ubx-dbus --load-usc` sends the usc text to the node, which loads it
in `lsdb-intf`. Both sides change:

- `LoadUSCLua(s)` becomes `LoadUSCLua(s, a{ss}) -> as`, returning
  the warnings. This is a breaking D-Bus interface change; callers
  pass an empty dict for no parameters. `load_usc_lua()` in `lsdb-intf.lua` opens the context
  around `load_str()`, and unknown-name and conflict errors are
  returned as D-Bus errors. `LoadUSCJSON` is unchanged, JSON models
  cannot declare parameters.
- `ubx-dbus` gets `-D` (same syntax as `ubx-launch`) and passes the
  values with `--load-usc`, and prints the returned warnings. `-D`
  without `--load-usc`, or with a `.json` model, is an error.
- `ubx-dbus --load-usc=FILE --params` lists the parameters locally:
  it loads the file inside a context, does not launch it and does not
  contact the node. Nested `bd.load()` paths resolve against the
  client's working directory here, not the node's.

Changes
-------

| File                                 | Change                                          | Size       |
|--------------------------------------|-------------------------------------------------|------------|
| `lua/blockdiagram.lua`               | `bd.param()`, context, `with_params()`, helpers | ~190 lines |
| `tools/ubx-launch`                   | `-D`, `--params`, help text                     | ~40 lines  |
| `std_blocks/lsdb-intf/lsdb-intf.lua` | `LoadUSCLua(s, a{ss}) -> as`                    | ~10 lines  |
| `std_blocks/lsdb-intf/ubx-dbus`      | `-D`, `--params`                                | ~40 lines  |
| docs                                 | README, `composing_systems.rst`, ChangeLog      | ~70 lines  |
| tests                                | `test_params.lua`, `test_lsdb_intf.lua`         | ~600 lines |

Compatibility
-------------

- `bd.load()`/`bd.load_str()` keep their signatures. Callers that
  load outside a context get the defaults.
- a usc load error now reads `failed to load <file>` (`<string>` for
  `load_str()`) instead of `failed to load usc`, so an error in a
  nested submodel names each file.
- models that keep using `os.getenv()` are unaffected. `bd.param()`
  does not fall back to `os.getenv()`: that would keep the unchecked
  path alive.
- `LoadUSCLua` changes its signature, see ubx-dbus above.

Limitations
-----------

- declarations must be unconditional, i.e. at the top of the model. A
  `bd.param()` inside a branch that is not taken, or in a submodel
  that is included conditionally, leaves its name undeclared, and
  passing it then fails as unknown. Document as a convention.
- `--params` shows only what the loaded files declare; with
  conditional declarations it is incomplete for the same reason.
- no per-instance values for a submodel included more than once.
