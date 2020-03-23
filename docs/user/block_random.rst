Module random
-------------

Block random/random
^^^^^^^^^^^^^^^^^^^

| **Type**:       cblock
| **Attributes**: 
| **Meta-data**:  { doc='A random number generator function block',  realtime=true,}
| **License**:    BSD-3-Clause


Configs
"""""""

.. csv-table::
   :header: "name", "type", "doc"

   loglevel, ``int``, ""
   min_max_config, ``struct random_config``, ""



Ports
"""""

.. csv-table::
   :header: "name", "out type", "out len", "in type", "in len", "doc"

   seed, , , ``unsigned int``, 1, ""
   rnd, ``unsigned int``, 1, , , ""

Types
^^^^^

.. csv-table:: Types
   :header: "type name", "type class", "size [B]"

   ``struct random_config``, struct, 8


