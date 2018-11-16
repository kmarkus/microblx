API Changes
===========

This file tracks user visible API changes.

## v0.4.1

- static initialization of block configuration array length has
  changed during the refactoring for node config support:

```C
/* old way */
ubx_config_t blk_conf[] = {
	{ .name="foo", .type_name="char", .value = { .len=32 }
    { NULL }
};
```

```C
/* new way */
ubx_config_t blk_conf[] = {
	{ .name="foo", .type_name="char", .data_len=32 }
    { NULL }
};
```

  if you didn't use this (probably true for most blocks), there's
  nothing to change.

## v0.4.0

- `ubx_launch`: shutdown option `-b` added to shutdown when a
  (trigger) block is stopped.

- `ubx_data_free`: removed unused ni argument

- enabled full warnings `-Wall -Wextra -Werror` and fixed all
  occurences.

- added `ubx_outport_add` and `ubx_inport_add` convenience functions.

- thorough cleanup of `ptrig`:
  - removed ifdef'ed timing statistics
  - removed MAX_BLOCKS define for number of timing blocks
  - `tstats_print_on_stop` config to print statistics


- `ubx_types.h`: reduce `BLOCK_NAME_MAXLEN` to 30

- more tests added: ptrig

- various fixes

## v0.3.4

- `ramp` block generalized to support all basic types

- various fixes

## v0.3.4

- simple ramp block added

- `blockmodel`: pulldown support added.

- promoted `get_num_configs` and `get_num_ports` to public IF.

- `ubx_ts_to_ns` added to convert `ubx_timespec` to uint64_t ns.

- `ubx_launch`: -t added to run for a specific amount of time.

## v0.3.3

- `ubx_launch` start directive
  e5a716c4c096020ec0767a7eb87c3952977919a2

- `ubx_launch` JSON support 4cc5fc83ae966153289c4dc9733d77737f7342e1

- various smaller cleanups

## < v0.3.2
- support for type registration in C++ added
  0736a5172e7fb2a93dc7c9f42c525c0e5626ae0e

- introduce `ubx_ilaunch` for interactive mode
  1725f6172d92730044928ffaf984ec8b57be8bcb. Renamed non interactive
  version to `ubx_launch`.

- `file2carr.lua` renamed to `ubx_tocarr`
  a3eee190b93423a49a678d20b239bc1dd30a3444

- install core modules into `$(prefix)/lib/microblx/<major.minor>/`
  51e1bbb6c79dc3aa9081802334c54f2c5bab1270

- [9ff5615f9d] removed std Lua modules provided by separate package
  `uutils` (see README.md).

- [469e930b8a] Clang dependency removed: compiling with gcc works when
   using initialization as demonstrated in `std_blocks/cppdemo`.

- [4213621d55] merged support for autotools build

- [4e3a0b9a72] OO support for node, block, port, config, data and
  type. Removed old and unused Lua functions.

- [65cd7088c6] interaction blocks are not strongly typed, i.e. instead
  if data element size and number of buffer elements, the type name,
  array len and number of buffer elements are given. This will help
  ensure type safety for inter-process iblocks.

- [575d26d8ea] removed `ubx_connect_one` and `ubx_connect`. Use
  `ubx_connect_out`, `ubx_connect_in` and `ubx_connect_uni` instead.

- the port direction attrs `PORT_DIR_IN/OUT/INOUT` can be ommitted in
  the definition of block ports. They are automatically inferred from
  the presence of `in_type_name` / `out_type_name`.

- renamed `module_init/module_cleanup` to
  `UBX_MODULE_INIT/UBX_MODULE_CLEANUP`
