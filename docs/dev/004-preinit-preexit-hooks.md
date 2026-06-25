preinit / preexit life-cycle hooks
==================================

2026-06-25, mk

Problem
-------

Two recurring needs could not be served by the `init`..`cleanup`
hooks:

- a config whose value drives the creation of *other* configs
- a config whose value drives how ports are created (arraylen, type)

Doing this in `start` is wrong: ports must exist before
`connect_blocks`, which runs *before* `start`. Doing it in `init`
works only when the driving config is static, because a config that is
itself created in `init` only gets its value in the post-`init` config
pass — too late to size/create ports (which must happen while in
`preinit`).

The root cause: there was exactly one structural-mutation point
(`init`) with config applied before and after it, so dependent
creation could only go one level deep.

Solution
--------

Add an optional `preinit` hook (and its teardown counterpart
`preexit`) as a second structural-mutation point, earlier in the
`preinit` residence:

```
apply_config #1 (static)  ->  preinit()  ->  apply_config #2 (preinit-created)
  ->  init()  ->  reapply_config #3 (init-created)
```

`preinit` and `init` both run while the block is in `preinit` (the
state flips to `inactive` only *after* `init` returns), so both may
add/resize ports. The extra config pass between them lets a config
created by `preinit` be filled before `init` reads it.

Design decisions
----------------

- **Transition effect, not entry/exit action.** Like the existing
  hooks. An entry action of `preinit` would re-fire `preinit()` every
  time the block falls back to `preinit` during teardown (same reason
  `init` can't be an entry action of `inactive`: `stop` also enters it).

- **No new state.** The hooks live inside the existing `preinit`
  residence. Purely additive: optional function pointers (NULL ==
  today's behavior), and `ubx_block_init` runs `preinit` automatically
  (idempotent via the `preinited` flag), so any init path is safe.

- **`preexit` only on the `preinit -> [*]` (rm) edge**, not on
  `inactive -> preinit` (cleanup). So a block may cycle
  `cleanup -> init` keeping its preinit-created interface; the
  additions are torn down only on removal.

- **ABI:** the change grows `ubx_proto_block_t` / `ubx_block_t`. Source
  is additive but blocks must be recompiled against the new headers.

Statechart figure: deliberate inaccuracy
-----------------------------------------

`docs/user/_static/life_cycle.svg` draws the config-apply passes as
effects on the transitions, and puts `apply_config #1` + `preinit()`
on the `create -> preinit` edge. This is a simplification:

- Config application is **not** a per-edge step — it happens in a
  global phase *after all blocks are created* (so configs can
  cross-reference other blocks). It is drawn on edges only to convey
  ordering relative to the hooks.
- The *ordering* shown is exact:
  `create < apply#1 < preinit() < apply#2 < init() < reapply#3`.

The original FSM hid config application entirely; showing it on the
edges is strictly more informative, at the cost of this one abstraction.
The precise temporal view is the sequence diagram in
`docs/user/_static/launch_sequence.puml`.
