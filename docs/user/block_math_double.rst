Module math_double
------------------

Block ubx/math_double
^^^^^^^^^^^^^^^^^^^^^

| **Type**:       cblock
| **Attributes**: 
| **Meta-data**:   { doc='math functions from math.h',   realtime=true,}
| **License**:    BSD-3-Clause


Configs
"""""""

.. csv-table::
   :header: "name", "type", "doc"

   func, ``char``, "math function to compute"
   data_len, ``long``, "length of output data (def: 1)"
   mul, ``double``, "optional factor to multiply with y (def: 1)"
   add, ``double``, "optional offset to add to y after mul (def: 0)"



Ports
"""""

.. csv-table::
   :header: "name", "out type", "out len", "in type", "in len", "doc"

   x, , , ``double``, 1, "math input"
   y, ``double``, 1, , , "math output"



