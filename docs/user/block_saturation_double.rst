Module saturation_double
------------------------

Block ubx/saturation_double
^^^^^^^^^^^^^^^^^^^^^^^^^^^

| **Type**:       cblock
| **Attributes**: 
| **Meta-data**:  double saturation block
| **License**:    BSD-3-Clause


Configs
"""""""

.. csv-table::
   :header: "name", "type", "doc"

   data_len, ``long``, "data array length"
   lower_limits, ``double``, "saturation lower limits"
   upper_limits, ``double``, "saturation upper limits"



Ports
"""""

.. csv-table::
   :header: "name", "out type", "out len", "in type", "in len", "doc"

   in, , , ``double``, 1, "input signal to saturate"
   out, ``double``, 1, , , "saturated output signal"



