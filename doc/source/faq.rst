Frequently asked questions
==========================

fix “error object is not a string”
----------------------------------

Note that this has been fixed in commit ``be63f6408bd4d``.

This is most of the time happens when the ``strict`` module being loaded
(also indirectly, e.g. via ubx.lua) in a luablock. It is caused by the C
code looking up a non-existing global hook function. Solution: either
define all hooks or disable the strict module for the luablock.


``blockXY``.so or ``liblfds611.so.0``: cannot open shared object file: No such file or directory
------------------------------------------------------------------------------------------------

There seems to be a `bug
<https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=684981>`_ in some
versions of libtool which leads to the ld cache not being updated. You
can manually fix this by running

.. code:: sh

	$ sudo ldconfig


Often this means that the location of the shared object file is not in
the library search path. If you installed to a non-standard location,
try adding it to ``LD_LIBRARY_PATH``, e.g.

.. code:: sh

   $ export LD_LIBRARY_PATH=/usr/local/lib/

It would be better to install stuff in a standard location such as
``/usr/local/``.

Real-time priorities
--------------------

To run with realtime priorities, give the luajit binary ``cap_sys_nice``
capabilities, e.g:

.. code:: sh

   $ sudo setcap cap_sys_nice+ep /usr/local/bin/luajit-2.0.2

My script immedately crashes/finishes
-------------------------------------

This can have several reasons:

-  You forgot the ``-i`` option to ``luajit``: in that case the script
   is executed and once completed will immedately exit. The system will
   be shut down / cleaned up rather rougly.

-  You ran the wrong Lua executable (e.g. a standard Lua instead of
   ``luajit``).

Typically the best way to debug crashes is using gdb and the core dump
file:

.. code:: sh
	  
	  # enable core dumps
	  $ ulimit -c unlimited
	  $ gdb luajit
	  ...
	  (gdb) core-file core
	  ...
	  (gdb) bt
