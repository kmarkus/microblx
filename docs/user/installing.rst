Installing
==========

Building from source
--------------------

Dependencies
~~~~~~~~~~~~

**Mandatory** (apt):

.. code:: sh

   apt install cmake pkg-config luajit libluajit-5.1-dev uthash-dev \
       libsystemd-dev libmxml-dev

**Mandatory** (from source):

- ``ffi-reflect``: ffi reflection module — `ffi-reflect git <https://github.com/corsix/ffi-reflect>`_
- ``uutils``: Lua utilities — `uutils git <https://github.com/kmarkus/uutils>`_

**Optional blocks** — install the corresponding dependencies before
building to enable these blocks:

.. list-table::
   :header-rows: 1
   :widths: 20 30 50

   * - Block
     - Dependency
     - How to install
   * - ``lsdb-intf``
     - lsdbus (from source)
     - ``git clone https://github.com/kmarkus/lsdbus.git``; see below
   * - ``webgraph``
     - lua-socket, json.lua
     - ``apt install lua-socket lua-json``
   * - ``ubx/gps``
     - libgps
     - ``apt install libgps-dev gpsd``
   * - ``ubx/gpio``
     - libgpiod >= 2.0
     - ``apt install libgpiod-dev``
   * - ``ubx/iio``, ``ubx/iio_buf``
     - libiio >= 0.21
     - ``apt install libiio-dev libiio-utils``

``lsdb-intf`` also requires enabling at cmake time:
``cmake -DBLOCK_LSDB_INTF=ON ..``

Install ``lsdbus`` from source:

.. code:: sh

   git clone https://github.com/kmarkus/lsdbus.git
   cd lsdbus && mkdir build && cd build
   cmake .. -DCONFIG_LUA_VER=jit
   make -j$(nproc) && sudo make install

**Optional** (testing):

- ``lua-unit`` (apt: ``lua-unit``, `git <https://github.com/bluebird75/luaunit>`_)

Building
~~~~~~~~

Clone the code:

.. code:: bash

   $ git clone https://gitlab.com/kmarkus/microblx.git
   $ git clone https://github.com/kmarkus/uutils.git
   $ git clone https://github.com/corsix/ffi-reflect.git

Install *uutils*:

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
