Getting started
===============

Microblx in a nutshell
----------------------

Microblx is a lightweight framework to build function block based
systems.  It is designed around a canonical component model with
*ports* for data exchange, *configs* for configuration and a *state
machine* for the "block" life cycle. Trigger blocks orchestrate the
execution of the components functionality.

Building a microblx application typically involves two steps:

Implement the required blocks
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

define the **block API**

  - *configs*: what is (statically) configurable
  - *ports*: which data flows in and out of the block


and implement the required block **hooks**

  - For example, ``init`` is typically used to initialize, allocate memory and/or validate configuration.
  - the ``step`` hook implements the "main" functionality and is executed when the block is **triggered**.
  - ``cleanup`` is to "undo" ``init`` (i.e. free resources etc.)

Take a look at a simple `threshold checking
<https://github.com/kmarkus/microblx/blob/dev/std_blocks/examples/threshold.c>`_
demo block.

.. note::
   You can examine a block interface using ``ubx-modinfo``, e.g. run
   ``$ ubx-modinfo show threshold``.

Define the application using a usc file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This involves specifying

- which *block instances* to create
- the *configuration* of for each block
- the *connections* to create between ports
- the *triggering* of blocks (i.e. the schedule of when to trigger the step functions of each block)

A small, ready to run usc demo using the `threshold` block is
available `here
<https://github.com/kmarkus/microblx/blob/dev/examples/usc/threshold.usc>`_

`usc` applications can be launched using the `ubx-launch` tool, as
shown in the following example.


Run the threshold example
-------------------------

In this small example, a ramp feeds a sine generator whose output is
checked whether it exceeds a threshold. The threshold block outputs
the current state (1: *above* or 0: *below*) as well as *events* upon
passing the threshold. The events are connected to a mqueue block,
where they can be logged using the ``ubx-mq`` tool. The actual
composition looks as follows

.. code:: text

   /------\    /-----\    /-----\    /----\
   | ramp |--->| sin |--->|thres|--->| mq |
   \------/    \-----/    \-----/    \----/
      ^           ^          ^
      .           . #2       .
   #1 .           .          .
      .        /------\      . #3
      .........| trig |.......
               \------/

   ---> depicts data flow
   ...> means "triggers"


Before launching, start a ubx logger client in a separate terminal:

.. code:: sh

   $ ubx-log
   waiting for rtlog.logshm to appear

.. note::
   The following assumes microblx was installed in the default
   locations under ``/usr/local/``. If you installed it in a different
   location you will need to adapt the path.

Then in a new terminal:

.. code:: sh

   $ ubx-launch -loglevel 7 -c /usr/local/share/ubx/examples/usc/threshold.usc
   core_prefix: /usr/local
   prefixes:    /usr, /usr/local


We increase the loglevel to 7 (DEBUG) so that debug messages will be
visible. In the log window you should now see "threshold passed"
messages.

As the events output by the ``thres`` block are made available via a
``mqueue``, these can easily be dumped to stdout using ``ubx-mq``:

.. code:: sh

   $ ubx-mq read threshold_events -p threshold
   {ts={nsec=135724534,sec=287814},dir=1}
   {ts={nsec=321029297,sec=287814},dir=0}
   {ts={nsec=448964856,sec=287815},dir=1}

To stop the application again, just type ``Ctrl-c`` in the
``ubx-lauch`` window.


Run the PID controller block
----------------------------

This more complex example demonstrates how multiple, modular ``usc``
files can be *composed* into an application and how configuration can
be *overlayed*. The use-case is a robot controller composition which
shall be used in a test mode (extra mqueue ouputs, no real-time
priorities) and in regular mode (real-time priorities, no debug
outputs).

Before launching, run ``ubx-log`` as above to see potential errors.

Then:

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
the application. Browser to http://localhost:8888 and explore:

1. clicking on the node graph will show the connections
2. clicking on blocks will show their interface
3. start the ``file_log1`` block to enable logging
4. start the ``ptrig1`` block to start the system.


Examining data-flow
~~~~~~~~~~~~~~~~~~~

The ``pid_test.usc`` creates several mqueue blocks in order to export
internal signals for debugging. They can be accessed using the
``ubx-mq`` tool:

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


Important concepts
------------------

The following concepts are important to know:

- **modules** are shared libraries that contain blocks or custom types
  and are loaded when the application is launched.

- a **node** is a run-time container into which *modules* are loaded
  and which keeps track of blocks etc.

- **types**: microblx essentially uses the C type system (primitive
  types, structs and arrays of both) for `configs` and data sent via
  `ports`. To be supported by tools (that is in `usc` files or by
  tools like `ubx-mq`), custom types must be registered with
  microblx. The `stdtypes` module contains a large number of common
  types like `int`, `double`, stdints (`int32_t`) or time handling
  `ubx_tstat`.

- **cblocks** vs **iblocks**: there are two types of blocks: *cblocks*
  (computation blocks) are the "regular" functional blocks with a
  `step` hooks. In contrast *iblocks* (interaction blocks) are used to
  implement communication between blocks and implement `read` and
  `write` hooks. For most applications the available iblocks are
  sufficient, but sometimes creating a custom one can be useful.

- **triggers**: *triggers* are really just cblocks with a
  configuration for specifying a schedule and other properties such as
  period, thread priority, etc. `ptrig` is the most commonly used
  trigger which implements a periodic, POSIX pthread based
  trigger. Sometimes it is useful to implement custom triggers that
  trigger based on external events. The `trig_utils` functions (see
  `./libubx/trig_utils.h`) make this straightforward.

- **dynamic block interface**: sometimes the type or length of the
  port data is not static but depends on configuration values
  themselves. This is almost always the case for iblocks
