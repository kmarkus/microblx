Composing microblx systems
==========================

To build a microblx application, blocks need to be instantiated,
configured, connected and started up according to the life-cycle
FSM. There are different ways to do this:

- by using microblx system composition files (the ``*.usc`` files
  under examples). ``usc`` stands for microblx system
  composition). These can be launched using the ``ubx_launch``
  tool. For example:

.. code:: bash

   $ ubx_launch -webif -c examples/trig_rnd_hexdump.usc
 
  this will launch the given system composition and additionally
  create a webserver block to allow system to be introspected via a
  browser. Using the usc files is the preferred approach.

- by writing a Lua called “deployment script” (e.g. see
  ``examples/trig_rnd_to_hexdump.lua``). This is not recommended under
  normal circumstances, but can be useful in specific cases such as
  for building dedicated test tools.

- by assembling everything in C/C++. Possible, but somewhat painful to
  do by hand. This would be better solved by introducing a
  usc-compiler tool.


