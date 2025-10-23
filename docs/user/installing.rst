Installing
==========

Building from source
--------------------

Dependencies
~~~~~~~~~~~~

Make sure to install the following dependencies

- uthash (apt: ``uthash-dev``)
- cmake
- luajit (>=v2.0.0) (apt: ``luajit`` and ``libluajit-5.1-dev``)

The following must be installed from source (see instructions below):

- ``ffi-reflect`` ffi reflection module `ffi-reflect git <https://github.com/corsix/ffi-reflect>` 
- ``uutils`` Lua utilities `uutils git <https://github.com/kmarkus/uutils>`_
- ``liblfds`` lock free data structures (v6.1.1) `liblfds6.1.1 git <git://liblfds.org/git/liblfds6.1.1>`_(optional)


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

   $ git clone git://liblfds.org/git/liblfds6.1.1
   $ git clone https://github.com/kmarkus/microblx.git
   $ git clone https://github.com/kmarkus/uutils.git
   $ git clone https://github.com/corsix/ffi-reflect.git


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


Install *ffi-reflect*:

.. code:: bash

	  $ cp ffi-reflect/reflect.lua /usr/local/share/lua/5.1/

Now build *microblx*:

.. code:: bash

	  $ cd ../microblx
	  $ mkdir build && cd build
	  $ cmake ..
	  $ make
	  $ sudo make install

Using yocto
-----------

If you are developing for an embedded system, the recommended way is
use the `meta-microblx <https://github.com/kmarkus/meta-microblx>`_
yocto layer. Please see the README in that repository for further
instructions.
