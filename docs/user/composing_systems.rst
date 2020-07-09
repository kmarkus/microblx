Composing microblx systems
==========================

Building a microblx application typically involves instantiating
blocks, configuring and interconnecting their ports and finally
starting all blocks. The recommended way to do this is by specifying
the system using the microblx composition DSL.


Microblx System Composition DSL (usc files)
-------------------------------------------

``usc`` are declarative descriptions of microblx systems that can be
validated and instantiated using the ``ubx-launch`` tool. A usc model
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
	      { name="ptrig1", type="ubx/ptrig" },
	      ...
	   },

	   -- connect blocks
	   connections = {
	      { src="x1.out", tgt="y1.in" },
	      { src="y1.out", tgt="x1.in", buffer_len=16 },
	   },

	   -- configure blocks
	   configurations = {
	      { name="x1", config = { cfg1="foo", cfg2=33.4 } },
	      { name="y1", config = { cfgA={ p=1,z=22.3 }, cfg2=33.4 } },

	      -- configure a trigger
	      { name="trig1", config = { period = {sec=0, usec=100000 },
					 sched_policy="SCHED_OTHER",
					 sched_priority=0,
					 chain0={
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

usc files like the above example can be launched using ``ubx-launch``
tool. Run with ``-h`` for further information. The following
example

.. code:: sh
	  
   $ cd /usr/local/share/ubx/examples/usc/pid/
   $ ubx-launch -webif -c pid_test.usc,ptrig_nrt.usc
   ...

will launch the given system composition and in addition create and
configure a web server block to allow the system to be introspected
via browser.

Unless the ``-nostart`` option is provided, all blocks will be
initialized, configured and started. ``ubx-launch`` handles this in
safe way by starting up active blocks after all other blocks (In
earlier versions, there was ``start`` directive to list the blocks to
be started, however now this information is obtained by means of the
block attributes ``BLOCK_ATTR_ACTIVE`` and ``BLOCK_ATTR_TRIGGER``.)


Node configs
~~~~~~~~~~~~

Node configs allow to assign the same configuration to multiple
blocks. This is useful to avoid repeating global configuration values
that are identical for multiple blocks.

The ``node_configurations`` keyword allows to define one or more named
node configurations.

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


Hierarchical compositions
-------------------------

Using hierarchical composition [#f1]_ an application can be composed
from other compositions. The motivation is to permit reuse of the
individual compositions.

The ``subsystems`` keyword accepts a list of namespace-subsystem
entries:

.. code:: lua

	  return bd.system {
	      import = ...
	      subsystems = {
		  subsys1 = bd.load("subsys1.usc"),
		  subsys2 = bd.load("subsys1.usc"),
	      }
	  }

Subsystem elements like `configs` can be accessed by higher levels by
adding the subsystem namespace. For example, the following lines
override a configuration value of the ``blk`` block in subsystems
``sub11`` and ``sub11/sub21``:

.. code:: lua

	  configurations = {
	      { name="sub11/blk",       config = { cfgA=1, cfgB=2 } },
	      { name="sub11/sub21/blk", config = { cfgA=5, cfgB=6 } },
	  }

Note how the subsystem namespaces prevent name collisions of the two
identically names blocks. Similar to configurations, connections can
be added among subsystems blocks:

.. code:: lua
	  
	  connections = {
	      { src="sub11/sub21/blk.portX", tgt="sub11/blk.portY" },
	  },


When launched, a hierarchical system is instantiated in a similar way
to a non-hierarchical one, however:

* modules are only imported once
* blocks from all hierarchy levels are instantiated, configured and
  started together, i.e. the hierarchy has no implications on the
  startup sequence.
* microblx block names use the fully qualified name including the
  namespace. Therefore, the #blockname syntax for resolving block
  pointers works just the same.
* if multiple configs for the same block exist, only the highest one
  in the hierarchy will be applied.
* node configs are always global, hence no prefix is required. In case
  of multiple identically named node configs, the one at the highest
  level will be selected.


Model mixins
------------

To obtain a reusable composition, it is important to avoid introducing
platform specifics such as ``ptrig`` blocks and their
configurations. Instead, passive ``trig`` blocks can be used to
encapsulate the trigger schedule. ``ptrig`` or similar active blocks
can then be added at *launch time* by merging them (encapsulated in an
usc file) into the primary model by specifying both on the
``ubx-launch`` command line.

For example, consider the example in
``examples/systemmodels/composition``:

.. code:: sh
	  
	  ubx-launch -webif -c deep_composition.usc,ptrig.usc


Alternatives
------------

Although using ``usc`` model is the preferred approach, there are
others way to launch a microblx application:

Launching in C
~~~~~~~~~~~~~~

It is possible to avoid the Lua scripting layer entirely and launch an
application in C/C++. A small self-contained example
:download:`c-launch.c <../../examples/C/c-launch.c>` is available under
``examples/C/`` (see the ``README`` for further details).

For a more complete example, checkout the respective tutorial section
:ref:`c-deployment`. Please note that such launching code is a likely
candidate for code generation and there are plans for a *usc-to-C*
compiler. Please ask on the mailing if you are interested.

Lua scripts
~~~~~~~~~~~

One can write a Lua "deployment script" similar to the
``ubx-launch``. Checkout the scripts in the ``tools`` section. This
approach not recommended under normally, but can be useful in specific
cases such as for building dedicated test tools.

.. rubric:: Footnotes

.. [#f1] This feature was introduced in the context of the COCORF
	 RobMoSys Integrated Technical Project. Please see
	 `docs/dev/001-blockdiagram-composition.md
	 <https://github.com/kmarkus/microblx/blob/cocorf/docs/dev/001-blockdiagram-composition.md>`_
	 for background information.
