API Changes
===========

This file tracks user visible API changes.

 * v0.3.3
   - ubx_launch start directive e5a716c4c096020ec0767a7eb87c3952977919a2
   - ubx_launch JSON support 4cc5fc83ae966153289c4dc9733d77737f7342e1
   - various smaller cleanups

 * support for type registration in C++ added
   0736a5172e7fb2a93dc7c9f42c525c0e5626ae0e

 * introduce `ubx_ilaunch` for interactive mode
   1725f6172d92730044928ffaf984ec8b57be8bcb. Renamed non interactive
   version to `ubx_launch`.

 * `file2carr.lua` renamed to `ubx_tocarr` a3eee190b93423a49a678d20b239bc1dd30a3444

 * install core modules into `$(prefix)/lib/microblx/<major.minor>/`
   51e1bbb6c79dc3aa9081802334c54f2c5bab1270

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
