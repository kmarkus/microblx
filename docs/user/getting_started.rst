Getting started
===============

Microblx in a nutshell
----------------------

Building a microblx application typically involves two steps:

**1. Implement the required blocks**

- define each *blocks API*:
  - *configs*: what is (statically) configurable
  - *ports*: which data flows in and out of the block
  
- implement the desired block *hooks*
  - For example `init` is typically used to initialize, allocate memory and/or validate configuration.
  - the `step` hook implements the "main" functionality and is executed when the block is *triggered*.
  - `cleanup` is to "undo" `init` (i.e. free resources etc.)

A minimal block example can be found under
`./std_blocks/minimal/threshold.c`

**2. Define the application using a usc file**

- which *block instances* to create
- the *configuration* of for each block
- the *connections* to create between ports
- the *triggering* of blocks (i.e. the schedule of when to trigger the step functions of each block)

A `usc` file can be directly launched using the `ubx-launch` tool.


Run the minimal threshold exxample
----------------------------------

NOTE: the following assumes microblx was installed in the default
locations under ``/usr/local/``. If you installed it in a different
location you will need to adopt the path to the examples.






Run the PID controller block
----------------------------

This example is to demonstrate a hierarchical controller composition
consisting of a PID controller and a trajectory controller (a simple
ramp).

Before launching the composition, run the log client to see potential
errors:

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

   
Important to know
-----------------

Some more concepts that are good to know:

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
