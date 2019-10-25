Introduction
============

Microblx is a lightweight function block model and implementation.

Key concepts
------------

- **block**: the basic building block of microblx. Is defined by filling in a
  ``ubx_block_t`` type and registering it with a microblx
  ``ubx_node_t``. Blocks *have* configuration, ports and operations.

  Each block is part of a module and becomes available once the module
  is loaded in a node.

  There are two types of blocks: **computation blocks** (“cblocks”,
  ``BLOCK_TYPE_COMPUTATION``) encapsulate “functionality” such as
  drivers and controllers. **interaction blocks** (“iblocks”,
  ``BLOCK_TYPE_INTERACTION``) are used to implement communication or
  interaction between blocks. This manual focusses on how to build
  cblocks, since this is what most application builders need to do.

  - **configuration**: defines static properties of blocks, such as
    control parameters, device file names etc.

  - **port**: define which data flows in and out of blocks.

- **type**: types of data sent through ports or of configuration must
  be registed with microblx.

- **node**: an adminstrative entity which keeps track of blocks and
  types. Typically one per process is used, but there’s no constraint
  whatsoever.

- **module**: a module contains one or more blocks or types that are
  registered/deregistered with a node when the module is
  loaded/unloaded.


Installation
------------

Dependencies
~~~~~~~~~~~~

 - uthash (apt: ``uthash-dev``)
 - luajit (>=v2.0.0) (apt: ``luajit`` and ``libluajit-5.1-dev``) (not
   strictly required, but recommended)
 - ``uutils`` Lua modules [github](https://github.com/kmarkus/uutils)
 - ``liblfds`` lock free data structures (v6.1.1) [github](https://github.com/liblfds/liblfds6.1.1)
 - ``lua-unit`` (apt: ``lua-unit``, src:
   [luaunit](https://github.com/bluebird75/luaunit) (to run the tests)
 - gcc or clang
 - only for development: ``cproto`` (apt: ``cproto``) to generate C prototype header file
 - autotools etc. (apt: ``automake``, ``libtool``, ``pkg-config``, ``make``)

Building and setting up
------------------------

Using yocto
~~~~~~~~~~~

The best way to use microblx on an embedded system is by using the
[meta-microblx](https://github.com/kmarkus/meta-microblx) yocto
layer. Please see the README in that repository for further steps.

Building manually
~~~~~~~~~~~~~~~~~

Building to run locally on a PC.

Before building microblx, liblfds611 needs to be built and
installed. There is a set of patches in the microblx repository to
clean up the packaging of liblfds. Follow the instructions below:

Clone the code:

.. code:: bash
   
   $ git clone https://github.com/liblfds/liblfds6.1.1.git
   $ git clone https://github.com/kmarkus/microblx.git
   $ git clone https://github.com/kmarkus/uutils.git


First build lfds:

.. code:: bash

	  $ cd liblfds6.1.1
	  $ git am ../microblx/liblfds/*.patch
	  $ ./bootstrap
	  $ ./configure --prefix=/usr
	  $ make
	  $ sudo make install

Then install ``uutils``:

.. code:: bash
	  
	  $ cd ../uutils
	  $ sudo make install


Now build microblx:

.. code:: bash
	  
	  $ cd ../microblx
	  $ ./bootstrap
	  $ ./configure
	  $ make
	  $ sudo make install



Quickstart
----------

Run the example system:

```bash
$ ubx_ilaunch -webif -c /usr/share/microblx/examples/systemmodels/trig_rnd_hexdump.usc
```
browse to http://localhost:8888 to inspect the system.


