API Changes
===========

This file tracks user visible API changes.

## unreleased

- `stattypes`: merge into `stdtypes` as this only contains the
  ubx_tstat type. There is hence no need to import `stattypes`
  anymore.

## v0.7.1

- core: add environment variable `UBX_PATH` that can be used to
  specify alternative installation paths. If defined, ubx will search
  the colon separated list of directories for ubx headers and modules.

- core: ffi: types: strip preprocessor directives from types that are
  fed to the ffi. This is to allow include guards in these header
  files.

- config: add `min`/`max` specifiers for introducing constraints such
  as optional or mandatory configs.

- struct ubx_block: added attributes variable (`attrs`) and defined
  first block attributes (`BLOCK_ATTR_TRIGGER` and
  `BLOCK_ATTR_ACTIVE`). These will be used by tools such as
  ubx_launch, so blocks should be updated to define these
  appropriately.

- blockdiagram.load: change to return `system` or fail and exit

- rtlog: enable gcc format checking. This checks the provided
  arguments match the given format string.

- ubx_tocarr: cmdline args added to allow overriding output file file

## v0.7.0

- ubxcore: `ubx_node_cleanup` changed to remove blocks and unload
  modules, but to leaving the node intact. For destroying a node,
  `ubx_node_rm` was introduced.

- ubx.lua: `config_get` returns `nil` if a config doesn't exist
  instead of throwing and error

- added `ubx-modinfo` tool for offline module introspection

- lfds_cyclic: increased loglevel for buffer overflow logging to
  NOTICE.

- introduced real-time safe logging and switched core and std_blocks
  to it

- `data_size`: changed return value to `long` (signed) to be able to
  signal errors via negative return value.

## v0.6.2

- renamed TSC configure option `tsc-timers` to `timesrc-tsc`

- added hooks for extra checking `-check` and `-werror` and two checks
  for unconnected in- and outports.

- removed generated `ubx_proto.h` from git

- cleanup: headers moved to `include/ubx/`. pkg-config file adapted,
  so this change should be transparent.

- trig, tstat: timing statistics support generalized for use in
  non-core blocks.

- support for using TSC timers (x86) as a low-overhead time source for
  timing statistics.

- `ubx.lua` use unversioned `libubx.so`

- bugfix in `ubx_port_add` (see 28e6d8acc42)


## v0.6.1

- ubx_launch, blockdiagram: exit and complain loudly if launching a
  blockdiagram fails

## v0.6.0

- updated all blocks to new config handling

- switch len and ptr* `ubx_config_get_data_ptr` to be streamlined with
  the new `cfg_getptr_*` utilities and to allow reporting errors. The
  new prototype is:

```C
 `long int ubx_config_get_data_ptr(ubx_block_t *b, const char *name,
 void **ptr)
 ```
  len < 0 signifies and error, 0 that the config is unset and > 0
  means configured and array lenght of `len`.

- configurations are not initalized to size 1 by default. This was
  useful back in the day when most configurations were of len 1, but
  this is not true anymore and it makes in cumbersome to distinguish
  between unconfigured and configurations with value 0. This is mainly
  visible through `ubx_config_get_data_ptr`, which may now return
  `NULL` and len=0. Blocks must handle this case.

- `cfg_getptr_[char|int|...]` convenience functions added. These
  functions allow retrieving configuration in a somewhat more typesafe
  manner. Example:

  ```C
  long int len;
  uint32_t *val;

  len = cfg_getptr_uint32(b, "myconf", &val);

  if(len == 0)
	  /* myconf unconfigured */
  else if(len > 1)
	  /* myconfig configured, use val */
  else
	  /* len < 0, error */
  ```

- removed unused and deprecated `lfds_cyclic_raw` block.

## v0.5.1

- blockdiagram, ubx_launch: moved node config from `configurations` to
  a separate top level section `node_configurations`. This avoids that
  `configurations` becomes a hybrid dictionary/array which causes
  problems when importing from json.

- pkg-config: `ubx_moddir` renamed to `UBX_MODDIR` for consistency.

## v0.5.0

- support for node configuration in C and via the `ubx_launch`
  DSL. Using the node (global) configuration mechanism multiple block
  configurations can be setup to refer to the same configuration
  value. This way, only one "screw" needs to be turned to change the
  configuration for all blocks, and the required memory is only
  allocated once.

  Note that it is transparent to the blocks whether they are
  configured individually or via a node config.

  For an example, take a look at
  [node_config_demo.usc](examples/systemmodels/node_config_demo.usc)

- the static initialization of block configuration array length has
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

  if you didn't use this (probably the case for most blocks), there's
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
