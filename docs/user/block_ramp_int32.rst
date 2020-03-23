Module ramp_int32
-----------------

Block ramp_int32
^^^^^^^^^^^^^^^^

| **Type**:       cblock
| **Attributes**: 
| **Meta-data**:   { doc='Ramp generator block',   realtime=true,}
| **License**:    BSD-3-Clause


Configs
"""""""

.. csv-table::
   :header: "name", "type", "doc"

   start, ``int32_t``, "ramp starting value (def 0)"
   slope, ``int32_t``, "rate of change (def: 1)"
   data_len, ``long``, "length of output data (def: 1)"



Ports
"""""

.. csv-table::
   :header: "name", "out type", "out len", "in type", "in len", "doc"

   out, ``int32_t``, 1, , , "ramp generator output"



