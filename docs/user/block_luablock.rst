Module luablock
---------------

Block lua/luablock
^^^^^^^^^^^^^^^^^^

| **Type**:       cblock
| **Attributes**: 
| **Meta-data**:  { doc='A generic luajit based block',  realtime=false,}
| **License**:    BSD-3-Clause


Configs
"""""""

.. csv-table::
   :header: "name", "type", "doc"

   lua_file, ``char``, ""
   lua_str, ``char``, ""
   loglevel, ``int``, ""



Ports
"""""

.. csv-table::
   :header: "name", "out type", "out len", "in type", "in len", "doc"

   exec_str, ``int``, 1, ``char``, 1, ""



