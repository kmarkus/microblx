API Changes
===========

This file tracks user visible API changes.

 * [575d26d8ea] removed ubx_connect_one and ubx_connect. Use
   ubx_connect_out, ubx_connect_in and ubx_connect_uni instead.

 * the port direction attrs PORT_DIR_IN/OUT/INOUT can be ommitted in
   the definition of block ports. They are automatically inferred from
   the presence of in_type_name / out_type_name.

 * renamed module_init/module_cleanup to UBX_MODULE_INIT/UBX_MODULE_CLEANUP

