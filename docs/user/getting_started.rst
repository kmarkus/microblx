Getting started
===============

Overview
--------

Microblx is a lightweight, hard real-time safe function block model
and implementation. Microblx applications are built from the following
primitives:

- **module**: a shared library that contains one or more blocks or
  types that are registered/deregistered with a *node* when the module
  is loaded/unloaded.

- **node**: a container for blocks and types. Keeps track of block
  instances cleans up during shutdown. Typically one node is used per
  process is used.

- **block**: the basic building block. Is defined by filling in a
  ``ubx_block_t`` type and registering it with a microblx
  ``ubx_node_t``. Blocks *have* configuration, ports and operations.

  Each block is part of a module and becomes available once the module
  is loaded in a node.

  There are two types of blocks: **computation blocks** (“cblocks”,
  ``BLOCK_TYPE_COMPUTATION``) encapsulate “functionality” such as
  drivers and controllers. **interaction blocks** (“iblocks”,
  ``BLOCK_TYPE_INTERACTION``) are used to implement communication or
  interaction between blocks. This manual focuses on how to build
  cblocks, since this is what most application builders need to do.

  Each block has zero or many of

  - **config**: defines any the static configuration blocks can have,
    such as control parameters, device file names etc.

  - **ports**: defines the type of the data involved in interactions
    between blocks (e.g. for data-flow).

- **type**: microblx essentially uses the C type system (primitive
  types and structs and arrays of the former) for `configs` and values
  sent via `ports`. To be usable via the scripting layer and the DSL,
  custom types must be registered with microblx. The `stdtypes` module
  contains generic, frequently types (e.g. stdints like `uint32` or
  time handling `ubx_tstat`).


Installation
------------

Dependencies
~~~~~~~~~~~~

- uthash (apt: ``uthash-dev``)
- luajit (>=v2.0.0) (apt: ``luajit`` and ``libluajit-5.1-dev``) (not strictly required, but recommended)
- ``uutils`` Lua utilities `uutils git <https://github.com/kmarkus/uutils>`_
- ``liblfds`` lock free data structures (v6.1.1) `liblfds6.1.1 git <https://github.com/liblfds/liblfds6.1.1>`_
- autotools etc. (apt: ``automake``, ``libtool``, ``pkg-config``, ``make``)
- ``cproto`` (apt: ``cproto``) use by Make to generate prototype header file
  
To run the tests:

- ``lua-unit`` (apt: ``lua-unit``, `git <https://github.com/bluebird75/luaunit>`_) (to run the tests)

Building and setting up
~~~~~~~~~~~~~~~~~~~~~~~

Using yocto
^^^^^^^^^^^

The best way to use microblx on an embedded system is by using the
`meta-microblx <https://github.com/kmarkus/meta-microblx>`_ yocto
layer. Please see the README in that repository for further steps.

Building manually
^^^^^^^^^^^^^^^^^

Building to run locally on a PC.

Before building microblx, liblfds611 needs to be built and
installed. There is a set of patches in the microblx repository to
clean up the packaging of liblfds. Follow the instructions below:

Clone the code:

.. code:: bash
   
   $ git clone https://github.com/liblfds/liblfds6.1.1.git
   $ git clone https://github.com/kmarkus/microblx.git
   $ git clone https://github.com/kmarkus/uutils.git


First build *lfds-6.1.1*:

.. code:: bash

	  $ cd liblfds6.1.1
	  $ git am ../microblx/liblfds/*.patch
	  $ ./bootstrap
	  $ ./configure
	  $ make
	  $ sudo make install

Then install *uutils*:

.. code:: bash
	  
	  $ cd ../uutils
	  $ sudo make install


Now build *microblx*:

.. code:: bash
	  
	  $ cd ../microblx
	  $ ./bootstrap
	  $ ./configure
	  $ make
	  $ sudo make install



Quickstart
----------

NOTE: the following assumes microblx was installed in the default
locations under ``/usr/local/``. If you installed it in a different
location you will need to adopt the path to the examples.

Run the PID controller block
----------------------------

This example is to demonstrate a hierarchical controller composition
consisting of a PID controller and a trajectory controller (a simple
ramp).

Before launching the composition, it is advisable to run the logging
client to see potential errors:

.. code:: sh

   $ ubx-log
   

and then in another terminal:

.. code:: sh

   $ cd /usr/local/share/ubx/examples/usc/pid/
   $ ubx-launch -webif -c pid_test.usc,ptrig_nrt.usc
   merging ptrig_nrt.usc into pid_test.usc
   core_prefix: /usr/local
   prefixes:    /usr, /usr/local
   starting up webinterface block (http://localhost:8888)
   loaded request_handler()

The `ubx-log` window will show a number messages from the
instantiation of the application. The last lines will be about the
blocks that were started.

Use the webif block
~~~~~~~~~~~~~~~~~~~

The cmdline arg ``-webif`` instructed ``ubx-launch`` to create a web
interface block. This block is useful for debugging and introspecting
the application:

Explore:

1. clicking on the node graph will show the connections
2. clicking on blocks will show their interface
3. start the ``file_log1`` block to enable logging
4. start the ``ptrig1`` block to start the system.


Examining data-flow
~~~~~~~~~~~~~~~~~~~

The ``pid_test.usc`` creates several mqueue blocks in order to export
internal signals for debugging. They can be accessed using the ``ubx-mq`` tool:

.. code:: sh

   $ ubx-mq list
   243b40de92698defa93a145ace0616d2  1    trig_1-tstats
   e8cd7da078a86726031ad64f35f5a6c0  10   ramp_des-out
   e8cd7da078a86726031ad64f35f5a6c0  10   ramp_msr-out
   e8cd7da078a86726031ad64f35f5a6c0  10   controller_pid-out

For example to print the ``controller_pid-out`` signal:

.. code:: sh

   ubx-mq  read controller_pid-out
   {1775781.9200001,1775781.9200001,1775781.9200001,1775781.9200001,1775781.9200001,1775781.9200001,1775781.9200001,1775781.9200001,1775781.9200001,1775781.9200001}
   {1776377.9200001,1776377.9200001,1776377.9200001,1776377.9200001,1776377.9200001,1776377.9200001,1776377.9200001,1776377.9200001,1776377.9200001,1776377.9200001}
   {1776974.0200001,1776974.0200001,1776974.0200001,1776974.0200001,1776974.0200001,1776974.0200001,1776974.0200001,1776974.0200001,1776974.0200001,1776974.0200001}
   {1777570.2200001,1777570.2200001,1777570.2200001,1777570.2200001,1777570.2200001,1777570.2200001,1777570.2200001,1777570.2200001,1777570.2200001,1777570.2200001}
   ...

