API Changes
===========

This file tracks user visible API changes.

 * support for type registration in C++ added

 * introduce `ubx_ilaunch` for interactive mode. Renamed non
   interactive version to `ubx_launch`

 * `file2carr.lua` renamed to `ubx_tocarr`

 * install core modules into `$(prefix)/lib/microblx/<major.minor>/`

 * [9ff5615f9d] removed std Lua modules provided by separate package
   uutils (see README.md).

 * [469e930b8a] Clang dependency removed: compiling with gcc works
   when using initialization as demonstrated in std_blocks/cppdemo.

 * [4213621d55] merged support for autotools build

 * [4e3a0b9a72] OO support for node, block, port, config, data and
   type. Removed old and unused Lua functions.

 * [65cd7088c6] interaction blocks are not strongly typed,
   i.e. instead if data element size and number of buffer elements,
   the type name, array len and number of buffer elements are
   given. This will help ensure type safety for inter-process iblocks.

 * [575d26d8ea] removed ubx_connect_one and ubx_connect. Use
   ubx_connect_out, ubx_connect_in and ubx_connect_uni instead.

 * the port direction attrs PORT_DIR_IN/OUT/INOUT can be ommitted in
   the definition of block ports. They are automatically inferred from
   the presence of in_type_name / out_type_name.

 * renamed module_init/module_cleanup to UBX_MODULE_INIT/UBX_MODULE_CLEANUP
