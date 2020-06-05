Module math_double
------------------

Block math_double
^^^^^^^^^^^^^^^^^

| **Type**:       cblock
| **Attributes**: 
| **Meta-data**:   { doc='mathmetric function block',   realtime=true,}
| **License**:    BSD-3-Clause


Configs
"""""""

.. csv-table::
   :header: "name", "type", "doc"

   func, ``char``, "math function to compute"
   data_len, ``long``, "length of output data (def: 1)"
   mul, ``double``, "optional factor to apply to result of func (def: 1)"
   add, ``double``, "optional offset to apply to result of func (def: 0)"



Ports
"""""

.. csv-table::
   :header: "name", "out type", "out len", "in type", "in len", "doc"

   x, , , ``double``, 1, "math input"
   y, ``double``, 1, , , "math output"



