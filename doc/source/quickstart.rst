Quickstart
==========

NOTE: the following assume microblx was installed in the default
locations under ``/usr/local/``. If you installed it in a different
location you will need to adopt the path to the examples.

Run the random block example
----------------------------

This (silly) example creates a random number generator block. It’s
output is hexdump’ed (using the ``hexdump`` interaction block) and also
logged using a ``file_logger`` block.

Before launching the composition, it is advisable to run the logging
client to see potential errors:

::

   $ ubx_log

and then in another terminal:

.. code:: sh

   $ ubx_ilaunch -webif -c /usr/local/share/ubx/examples/systemmodels/trig_rnd_hexdump.usc

Browse to http://localhost:8888

Explore:

1. clicking on the node graph will show the connections
2. clicking on blocks will show their interface
3. start the ``file_log1`` block to enable logging
4. start the ``ptrig1`` block to start the system.

Create your first block
-----------------------

Generate a block
~~~~~~~~~~~~~~~~

.. code:: sh

   $ ubx_genblock -d myblock -c /usr/local/share/ubx/examples/blockmodels/block_model_example.lua
       generating myblock/bootstrap
       generating myblock/configure.ac
       generating myblock/Makefile.am
       generating myblock/myblock.h
       generating myblock/myblock.c
       generating myblock/myblock.usc
       generating myblock/types/vector.h
       generating myblock/types/robot_data.h

Run ``ubx_genblock -h`` for full options.

The following files are generated:

-  ``bootstrap`` autoconf bootstrap script
-  ``configure.ac`` autoconf input file
-  ``Makefile.am`` automake input file
-  ``myblock.h`` block interface and module registration code (don’t
   edit)
-  ``myblock.c`` module body (edit and implement functions)
-  ``myblock.usc`` simple microblx system composition file, see below
   (can be extended)
-  ``types/vector.h`` sample type (edit and fill in struct body)
-  ``robot_data.h`` sample type (edit and fill in struct body)

Compile the block
~~~~~~~~~~~~~~~~~

.. code:: sh

   $ cd myblock/
   $ ./bootstrap
   $ ./configure
   $ make
   $ make install

Launch block using ubx_launch
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: sh

   $ ubx_ilaunch -webif -c myblock.usc

Run ``ubx_launch -h`` for full options.

Browse to http://localhost:8888
