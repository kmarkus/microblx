Module trig
-----------

Block ubx/trig
^^^^^^^^^^^^^^

| **Type**:       cblock
| **Attributes**: trigger
| **Meta-data**:  { doc='simple, activity-less trigger',  realtime=true,}
| **License**:    BSD-3-Clause


Configs
"""""""

.. csv-table::
   :header: "name", "type", "doc"

   num_chains, ``int``, "number of trigger chains. def: 1"
   tstats_mode, ``int``, "0: off (def), 1: global only, 2: per block"
   tstats_profile_path, ``char``, "directory to write the timing stats file to"
   tstats_output_rate, ``double``, "throttle output on tstats port"
   tstats_skip_first, ``int``, "skip N steps before acquiring stats"
   loglevel, ``int``, ""



Ports
"""""

.. csv-table::
   :header: "name", "out type", "out len", "in type", "in len", "doc"

   active_chain, , , ``int``, 1, "switch the active trigger chain"
   tstats, ``struct ubx_tstat``, 1, , , "timing statistics (if enabled)"



