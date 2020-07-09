Module ramp_double
------------------

Block ubx/ramp_double
^^^^^^^^^^^^^^^^^^^^^

| **Type**:       cblock
| **Attributes**: 
| **Meta-data**:   { doc='Ramp generator block',   realtime=true,}
| **License**:    BSD-3-Clause


Configs
"""""""

.. csv-table::
   :header: "name", "type", "doc"

   start, ``double``, "ramp starting value (def 0)"
   slope, ``double``, "rate of change (def: 1)"
   data_len, ``long``, "length of output data (def: 1)"



Ports
"""""

.. csv-table::
   :header: "name", "out type", "out len", "in type", "in len", "doc"

   out, ``double``, 1, , , "ramp generator output"



