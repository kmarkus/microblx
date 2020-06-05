Installing
==========

Building from source
--------------------

Dependencies
~~~~~~~~~~~~

Make sure to install the following dependencies

- uthash (apt: ``uthash-dev``)
- autotools etc. (apt: ``automake``, ``libtool``, ``pkg-config``, ``make``)
- ``cproto`` (apt: ``cproto``) use by Make to generate prototype
  header file
- luajit (>=v2.0.0) (apt: ``luajit`` and ``libluajit-5.1-dev``)

The following must be installed from source (see instructions below):

- ``uutils`` Lua utilities `uutils git <https://github.com/kmarkus/uutils>`_
- ``liblfds`` lock free data structures (v6.1.1) `liblfds6.1.1 git <https://github.com/liblfds/liblfds6.1.1>`_

Optionally, to run the tests:

- ``lua-unit`` (apt: ``lua-unit``, `git
  <https://github.com/bluebird75/luaunit>`_) (to run the tests)

Building
~~~~~~~~

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


Using yocto
-----------

If you are developing for an embedded system, the recommended way is
use the `meta-microblx <https://github.com/kmarkus/meta-microblx>`_
yocto layer. Please see the README in that repository for further
instructions.
