Composing microblx systems
==========================

Building a microblx application typically involves instantiating
blocks, configuring and interconnecting these and finally starting
these up. The recommended way to do this is by specifying the system
using the microblx composition DSL.


Microblx System Composition DSL (usc files)
-------------------------------------------

``usc`` are declarative descriptions of microblx systems that can be
validated and instantiated using the ``ubx_launch`` tool. A usc model
describes one microblx **system**, as illustrated by the following
minimal example:

.. code:: lua

	local bd = require("blockdiagram")

	return bd.system
	{
	   -- import microblx modules
	   imports = {
	      "stdtypes", "ptrig", "lfds_cyclic", "myblocks",
	   },

	   -- describe which blocks to instantiate
	   blocks = {
	      { name="x1", type="myblocks/x" },
	      { name="y1", type="myblocks/y" },
	      { name="ptrig1", type="std_triggers/ptrig" },
	      ...
	   },

	   -- connect blocks
	   connections = {
	      { src="x1.out", tgt="y1.in",  },
	      { src="y1.out", tgt="x1.in",  },
	   },

	   -- configure blocks
	   configurations = {
	      { name="x1", config = { cfg1="foo", cfg2=33.4 } },
	      { name="y1", config = { cfgA={ p=1,z=22.3 }, cfg2=33.4 } },

	      -- configure a trigger
	      { name="trig1", config = { period = {sec=0, usec=100000 },
					 sched_policy="SCHED_OTHER",
					 sched_priority=0,
					 trig_blocks={
					    -- the #<blockname> directive will
					    -- be resolved to an actual
					    -- reference to the respective
					    -- block once instantiated
					    { b="#x1", num_steps=1, measure=0 },
					    { b="#y1", num_steps=1, measure=0 } } } }
	   },
	}


Launching
~~~~~~~~~

usc files like the above example can be launched using ``ubx_launch``
tool. Run with ``-h`` for further information on the options. The
following simple example:

.. code:: bash

	  $ ubx_launch -webif -c examples/trig_rnd_hexdump.usc

this will launch the given system composition and in addition create
and configure a web server block to allow the system to be
introspected via a browser.

Unless the ``-nostart`` option is provided, all blocks will be
initialized, configured and started. ``ubx_launch`` takes care to do
this is safe way by starting up active blocks after all other blocks
(In earlier versions, there was ``start`` directive to list the blocks
to be started, however now this information is obtained by means of
the block attributes ``BLOCK_ATTR_ACTIVE`` and
``BLOCK_ATTR_TRIGGER``.)


Node configs
~~~~~~~~~~~~

Node configs allow to assign the same configuration to multiple
blocks. This is useful for global configuration values which are the
same for all blocks.

For this, a new top level section ``node_configurations`` is
introduced, which allows to defined named configurations.

.. code:: lua

     node_configurations = {
         global_rnd_conf = {
             type = "struct random_config",
	     config = { min=333, max=999 },
         }
     }


These configurations can then be assigned to multiple blocks:

.. code:: lua
	  
      { name="b1", config = { min_max_config = "&global_rnd_conf"} },
      { name="b2", config = { min_max_config = "&global_rnd_conf"} },


Please refer to ``examples/systemmodels/node_config_demo.usc`` for a
full example.


Alternatives
------------

Although using ``usc`` model is the preferred approach, there are
others way to launch a microblx application:

1. by writing a Lua called “deployment script” (e.g. see
   ``examples/trig_rnd_to_hexdump.lua``). This is not recommended
   under normal circumstances, but can be useful in specific cases
   such as for building dedicated test tools.

2. by assembling everything in C/C++. Possible, but somewhat painful
   to do by hand. This would be better solved by introducing a
   usc-compiler tool. Please ask on the mailing list.
