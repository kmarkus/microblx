Module pid
----------

Block pid
^^^^^^^^^

| **Type**:       cblock
| **Attributes**: 
| **Meta-data**:   { doc='',   realtime=true,}
| **License**:    BSD-3-Clause


Configs
"""""""

.. csv-table::
   :header: "name", "type", "doc"

   Kp, ``double``, "P-gain (def: 0)"
   Ki, ``double``, "I-gain (def: 0)"
   Kd, ``double``, "D-gain (def: 0)"
   data_len, ``long``, "length of signal array (def: 1)"



Ports
"""""

.. csv-table::
   :header: "name", "out type", "out len", "in type", "in len", "doc"

   msr, , , ``double``, 1, "measured input signal"
   des, , , ``double``, 1, "desired input signal"
   out, ``double``, 1, , , "controller output"



