# the new connect statement in usc files

2020-07-08, mk

## Overview

The existing blockdiagram `connection` statement is quite limited, as
it only supports connections via `lfds_cyclic` or "low-level"
connections to/from iblocks (which must be created manually
beforehand). The new connect statement shall support the following:

- allow specifying which iblock to connect with
- it must be possible to provide custom configs for iblocks
- allow iblock connections to/from existing and nonexisting iblocks
  (the latter need to be created, obviously)
- backwards compatibility as much as possible. By default connect via
  `lfds_cyclic`, `buffer_len = 1`

## Syntax

The new connect is as follows:

```Lua
connect { src="B1", tgt="B2", type="TYPE", config={...} }
```

## cblock connections

If `src` and `tgt` are cblocks, `connect` will create a connection
between the respective block ports. Both blocks *must* exist and have
the respective ports. In addition, the `type` of iblock to connect
with can be specified and the iblocks `config` provided.

The old toplevel connection field `buffer_len` is deprecated. Instead
this must now be defined (as any other iblock configuration) within
the config table.

The default when no type is defined is to use `lfds_cyclic` with a the
`buffer_len` set to 1.

## iblock connections

### connections to existing iblocks

Creating a cblock->iblock connection to an **existing** iblock is
achieved as follows:

```Lua
connect { src="CB1", tgt="IB1" } -- or
connect { src="IB1", tgt="CB1" }
```

**Note**:

- both src and tgt blocks must exist
- `type` and `config` are ignored (rationale: since the block was
  created elsewhere, it should also be configured elsewhere).

### connections to non-existing (to-be-created) iblocks

Create a cblock->iblock connection to a **non-existing** iblock use
the following:

```Lua
connect { src="CB1", tgt="IB1", type="IBTYPE", config={...} } -- or
connect { src="IB1", tgt="CB1", type="IBTYPE", config={...} }
```

**Note**:

- `CB1` *must* exit, `IB1` must **not**
- `type` must be a valid iblock type and `config` its config


## Implementation

`conn_lfds_cyclic` is dropped in favor of a higher level `connect`
function:

```Lua
function connect(nd, src, tgt, type, config)
```

which implements the specified behavior and can be directly used from
blockdiagram or scripts.
