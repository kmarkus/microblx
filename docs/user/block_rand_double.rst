Module rand_double
------------------

Block rand_double
^^^^^^^^^^^^^^^^^

| **Type**:       cblock
| **Attributes**: 
| **Meta-data**:   { doc='double random number generator block',   realtime=true,}
| **License**:    BSD-3-Clause


Configs
"""""""

.. csv-table::
   :header: "name", "type", "doc"

   seed, ``long``, "seed to initialize with"



Ports
"""""

.. csv-table::
   :header: "name", "out type", "out len", "in type", "in len", "doc"

   out, ``double``, 1, , , "rand generator output"



