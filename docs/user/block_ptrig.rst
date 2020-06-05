Module ptrig
------------

Block std_triggers/ptrig
^^^^^^^^^^^^^^^^^^^^^^^^

| **Type**:       cblock
| **Attributes**: trigger, active
| **Meta-data**:  { doc='pthread based trigger',  realtime=true,}
| **License**:    BSD-3-Clause


Configs
"""""""

.. csv-table::
   :header: "name", "type", "doc"

   period, ``struct ptrig_period``, "trigger period in { sec, ns }"
   stacksize, ``size_t``, "stacksize as per pthread_attr_setstacksize(3)"
   sched_priority, ``int``, "pthread priority"
   sched_policy, ``char``, "pthread scheduling policy"
   affinity, ``int``, "list of CPUs to set the pthread CPU affinity to"
   thread_name, ``char``, "thread name (for dbg), default is block name"
   trig_blocks, ``struct ubx_trig_spec``, "specification of blocks to trigger"
   autostop_steps, ``int64_t``, "if set and > 0, block stops itself after X steps"
   tstats_mode, ``int``, "enable timing statistics over all blocks"
   tstats_profile_path, ``char``, "directory to write the timing stats file to"
   tstats_output_rate, ``double``, "throttle output on tstats port"
   tstats_skip_first, ``int``, "skip N steps before acquiring stats"
   loglevel, ``int``, ""



Ports
"""""

.. csv-table::
   :header: "name", "out type", "out len", "in type", "in len", "doc"

   tstats, ``struct ubx_tstat``, 1, , , "out port for totals and per block timing statistics"
   shutdown, , , ``int``, 1, "input port for stopping ptrig"

Types
^^^^^

.. csv-table:: Types
   :header: "type name", "type class", "size [B]"

   ``struct ptrig_period``, struct, 16


