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

**Optional tools** — extend ``ubx-log`` with additional features:

.. list-table::
   :header-rows: 1
   :widths: 20 30 50

   * - Tool / feature
     - Dependency
     - How to install
   * - ``ubx-log -d`` (daemon mode)
     - libdaemon
     - ``apt install libdaemon-dev``

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

Compile time options
~~~~~~~~~~~~~~~~~~~~

The following cmake options change the behavior of the core:

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Option
     - Description
   * - ``-DENABLE_TIMESRC_TSC=ON``
     - use the x86 TSC instead of the POSIX clock as timesource. The
       conversion to time assumes a fixed CPU frequency, set with
       ``-DCPU_HZ=<hz>``
   * - ``-DENABLE_TIMESRC_CNTVCT=ON``
     - use the aarch64 ``CNTVCT`` counter as timesource (mutually
       exclusive with ``ENABLE_TIMESRC_TSC``)
   * - ``-DTRACING=SDT|MARKER``
     - build with tracing instrumentation, see ``libubx/ubx_trace.h``
   * - ``-DUBX_LOG_MSG_MAXLEN=<n>``
     - max length of a log message. This is part of the log shared
       memory layout, so writers and ``ubx-log`` must agree on it
   * - ``-DUBX_LOG_DAEMON=AUTO|ON|OFF``
     - build ``ubx-log`` with daemon mode (``-d``), which requires
       libdaemon
   * - ``-DCMAKE_BUILD_TYPE=<type>``
     - defaults to ``RelWithDebInfo``

The options a given installation was built with (plus the compiler,
target architecture and the detected scheduling features) can be
queried at runtime:

.. code:: bash

	  $ ubx-launch --version
	  microblx v0.9.2-154-gabcdef (modver 0.9)
	  build options:
	    timesrc:             POSIX
	    tracing:             OFF
	    log_msg_maxlen:      115
	    build_type:          RelWithDebInfo
	    compiler:            GNU 15.3.0
	    arch:                x86_64
	    sched_attr:          yes
	    sched_dl_overrun:    yes
	    pthread_setname:     yes
	    pthread_setaffinity: yes
	    module_dir:          /usr/local/lib/ubx/0.9/

``ubx-modinfo -version`` prints the same information. In addition, the
timesource and tracing backend are logged at ``INFO`` level on node
initialisation, so a captured log identifies the build it came from.

Using yocto
-----------

If you are developing for an embedded system, the recommended way is
use the `meta-microblx <https://github.com/kmarkus/meta-microblx>`_
yocto layer. Please see the README in that repository for further
instructions.
