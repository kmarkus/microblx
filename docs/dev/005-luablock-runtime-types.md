Runtime struct type registration from Lua blocks
================================================

2026-06-25, mk

Status: concept, not implemented.

Goal
----

Let a Lua block expose a real, typed (struct) config instead of
smuggling structured configuration through `lua_str`. Example:
`udpsink` declares its port set as a Lua table inside `lua_str`; a real
`struct`-array config would be typed, USC-settable per field, and
visible to tooling (`ubx-modinfo`, `lsdb-intf`, webgraph) — while
keeping the block a pure `ubx/luablock` (no separate C type module).

Why it works
------------

A `ubx_type_t` needs only `name`, `type_class`, `size`,
`private_data`. Two facts make runtime registration cheap:

- `private_data` of a struct type is just the C declaration string —
  `ffi_load_types` does `ffi.cdef(ffi.string(t.private_data))`.
- `ubx_type_register` computes the hash as `md5(name)` (name-only, not
  layout) and assigns `seqid` itself. No build-time `ubx-typegen`
  artifact is needed.

LuaJIT supplies the rest: `ffi.cdef(decl)`, then `ffi.sizeof(...)` for
`size`. No Lua `type_register` binding exists today — this is the
extension.

Flow (using the preinit hook, see 004)
--------------------------------------

```
preinit:  ffi.cdef(decl)
          ubx_type_register(nd, {name, size, STRUCT, private_data=decl})
          ubx.config_add(b, "ports", ..., "struct foo")
apply #2: deployment sets `ports = {...}` from the .usc (FFI marshals)
init:     read the struct config, build the interface
```

Suggested ergonomic helper: `ubx.type_add(nd, name, cdecl)` doing
cdef + sizeof + persistent-copy + register in one call.

Caveats (must handle)
---------------------

- **Pointer lifetime.** `name` and `private_data` are stored as raw
  `const char*`; a Lua string would be GC'd. Back them with persistent
  C memory (malloc + copy), freed in `preexit` / `type_unregister`.
- **`ffi.cdef` is process-global and append-only.** Same name +
  different layout clashes; a layout can't change across re-init.
  Namespace type names per block / dedup (as `ffi_load_types` already
  does).
- **Name-based hash => node-local.** A runtime type is meaningful only
  in this node's process; data crossing a boundary (mqueue, D-Bus,
  distributed) has no peer type. Fine for self-contained nodes only.
- **No type refcount.** `ubx_type_unregister` does not check use count;
  ensure no live data of the type exists before unregistering.
