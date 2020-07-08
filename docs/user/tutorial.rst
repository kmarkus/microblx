Tutorial: close loop control of a robotic platform
==================================================

Goal
----

This walk-through that shows how to:

1. write, compile and install two blocks:

   - the *plant* (a two DoF robot that accepts velocity as input, and
     gives the relative position), and
   - the *controller*, that given as a property the set-point and the
     gain, computes the desired velocity to be set to the robot.

2. instantiate the blocks via the ``ubx-launch`` tool, and
3. instantiate the blocks with a C program.

All the files can be found in the ``examples/platform`` folder.

Introductory steps
------------------

First of all, we need need to define the interface of and between our
two components.

The plant and controller have two ports, that exchange position and
velocity, each of dimension two, and some properties (initial position
and velocity limits for the plant, gain and setpoint for the
controller). These properties are described it two lua files:

The plant, ``platform_2dof.lua``

.. literalinclude:: ../../examples/platform/platform_2dof.lua
   :language: lua
	      
    
The controller, ``platform_2dof_control.lua``

.. literalinclude:: ../../examples/platform/platform_2dof_control.lua
   :language: lua

Let us have these file in a folder (*e.g.* microblx_tutorial). From
these file we can generate the two blocks using the the following bash
commands

.. code:: bash

   $ cd microblx_tutorial/
   $ ubx-genblock -c platform_2dof.lua -d platform_2dof
   generating platform_2dof/bootstrap
   ...
   $ ubx-genblock -c platform_2dof_control.lua -d platform_2dof_control
   generating platform_2dof_control/bootstrap
   ...

Each command generates a directory with the name specified after the
`-d` with six files. For the plant, we will have:

- ``boostrap``
- ``configure.ac``
- ``Makefile.am``
- ``platform_2dof.h``
- ``platform_2dof.c``
- ``platform_2dof.usc``

The only files we will modify are the C files ``platform_2dof.c`` and
``platform_2dof_control.c``.

Code of the blocks
------------------

The auto-generated files already give some hints on how to approach
the programming.

.. code:: c

	#include "platform_2dof.h"
	
	/* define a structure for holding the block local state. By assigning an
	 * instance of this struct to the block private_data pointer (see init), this
	 * information becomes accessible within the hook functions.
	 */
	struct platform_2dof_info
	{
		/* add custom block local data here */
	
		/* this is to have fast access to ports for reading and writing, without
		 * needing a hash table lookup */
		struct platform_2dof_port_cache ports;
	};
	
	/* init */
	int platform_2dof_init(ubx_block_t *b)
	{
		int ret = -1;
		struct platform_2dof_info *inf;
	
		/* allocate memory for the block local state */
		if ((inf = calloc(1, sizeof(struct platform_2dof_info)))==NULL) {
			ubx_err(b, "platform_2dof: failed to alloc memory");
			ret=EOUTOFMEM;
			goto out;
		}
		b->private_data=inf;
		update_port_cache(b, &inf->ports);
		ret=0;
	out:
		return ret;
	}
	
	/* start */
	int platform_2dof_start(ubx_block_t *b)
	{
		/* struct platform_2dof_info *inf = (struct platform_2dof_info*) b->private_data; */
	        ubx_info(b, "%s", __func__);
		int ret = 0;
		return ret;
	}
	
	/* cleanup */
	void platform_2dof_cleanup(ubx_block_t *b)
	{
		/* struct platform_2dof_info *inf = (struct platform_2dof_info*) b->private_data; */
	        ubx_info(b, "%s", __func__);
		free(b->private_data);
	}
	
	/* step */
	void platform_2dof_step(ubx_block_t *b)
	{
		/* struct platform_2dof_info *inf = (struct platform_2dof_info*) b->private_data; */
	        ubx_info(b, "%s", __func__);
	}
	

We will need then to insert the code indicated by the comments.

Step 1: add block state and helpers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

First add variables for storing the robot state, and implement the
other helper functions. At the beginning of the file we insert the
following code, to save the state of the robot:

.. literalinclude:: ../../examples/platform/platform_2dof.c
   :language: C
   :lines: 6-28

The ``last_time`` variable is needed to compute the time passed
between two calls of the ``platform_2dof_step`` function.

Step 2: Initialization and start functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``init`` function is called when the block is initialized; it
allocates memory for the info structure, caches the ports, and
initializes the state given the configuration values (these values are
specified in the ``.usc`` or main application file).

.. literalinclude:: ../../examples/platform/platform_2dof.c
   :language: C
   :lines: 30-67
	
The function

.. code:: c

	long cfg_getptr_double(ubx_block_t *b, const char *name, const double **ptr)

returns the address of the ``double`` configuration in the pointer
``ptr``. In this case the return value will be ``2`` (the length of
the data) or ``-1`` (failure, e.g. mistyped configuration
name). Because we set ``min`` and ``max`` in the configuration
declaration, we can be sure that at this point the array length is not
anything but 2.

In the start function we only need to initialize the internal timer

.. literalinclude:: ../../examples/platform/platform_2dof.c
   :language: C
   :lines: 69-75


Step 3: Step function
~~~~~~~~~~~~~~~~~~~~~

In the step function, we compute the time since last iteration, read the
commanded velocity, integrate to position, and then write position.

.. literalinclude:: ../../examples/platform/platform_2dof.c
   :language: C
   :lines: 84-118


In case there is no value on the port, a notice is logged and the
nominal velocity is set to zero. This will always happen for the first
trigger, since the controller did step yet and thus has not produced a
velocity command yet.

Step 4: Stop and clean-up functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

These functions are OK as they are generated, since the only thing we
want to take care of is that memory is freed.

Final listings of the block
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The plant is, *mutatis mutandis*, built following the same rationale,
and will be not detailed here. The final code of the plant and the
controller can be retrieved here:

- :download:`platform_2dof.c <../../examples/platform/platform_2dof.c>`
- :download:`platform_2dof_control.c <../../examples/platform/platform_2dof_control.c>`

Compiling the blocks
~~~~~~~~~~~~~~~~~~~~

In order to build and install the blocks, you must execute the following
bash commands in each of the two directories:

.. code:: bash

    $ ./bootstrap
    ...
    $ ./configure
    ...
    $ make
    ...
    $ sudo make install
    ...

See also the quickstart.

Deployment via the usc (microblx system composition) file
---------------------------------------------------------

``ubx-genblock`` generated sample ``.usc`` files to run each block
independently. We want to run and compose them together and make the
resulting signals available using message queues.  The composition
file **platform_2dof_and_control.usc** is quite self explanatory: It
contains

- the libraries to be imported,
- which blocks (name, type) to create,
- the configuration values of blocks.
- the connections among ports

The file :download:`platform_2dof_and_control.usc
<../../examples/platform/platform_launch/platform_2dof_and_control.usc>`
is shown below:

.. literalinclude:: ../../examples/platform/platform_launch/platform_2dof_and_control.usc
   :language: lua


It is worth noting that configuration types can be arrays (*e.g.*
``target_pos``), strings (``file_name`` and ``report_conf``) and
structures (``period``) and vector of structures (``chain0``). Types
can be checked using ``ubx-modinfo``:

.. code:: bash

    $ ubx-modinfo show platform_2dof
    module platform_2dof
      license: MIT

      blocks:
        platform_2dof [state: preinit, steps: 0] (type: cblock, prototype: false, attrs: )
	   configs:
	       joint_velocity_limits [double] nil //
               initial_position [double] nil //
           ports:
               pos  [out: double[2] #conn: 0] // measured position [m]
	       desired_vel [in: double[2] #conn: 0]  // desired velocity [m/s]

The file is launched with the command

.. code:: bash

   ubx-ilaunch -c platform_2dof_and_control.usc

or

.. code:: bash

   ubx-ilaunch -webif -c platform_2dof_and_control.usc

to enable the *web interface* at `localhost:8888 <localhost:8888>`__ .

To show the position and velocity signal, use the ``ubx-mq`` tool:

.. code:: bash

	$ ubx-mq list
	e8cd7da078a86726031ad64f35f5a6c0  2    vel_cmd
	e8cd7da078a86726031ad64f35f5a6c0  2    pos_msr
	
	$ ubx-mq read pos_msr
	{1.1,1}
	{1.13403850806,1.03503964065}
	{1.1679003576875,1.0698974270313}
	{1.2012522276799,1.1042302343764}
	{1.2342907518755,1.1382404798718}
	...

Some considerations about the fifos
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

First of all, consider that each (iblock) fifo can be connected to
multiple input and multiple output ports. Consider also, that if
multiple out are connected, if one block read one data, that data will
be consumed and not available for a second port.

The more common use-case is that each outport is connected to an
inport with it's own fifo. If the data that is produced by one outport
is needed to be read by two oe more inports, a fifo per inport is
connected to the the outport. **If you use the DSL, this is
automatically done, so you do not have to worry to explicitly
instantiate the iblocks. This also happens when adding ports to the
logger**.

.. **TODO insert picture**

Deployment via C program
------------------------

.. warning:: the following example is to illustrate the possibility of
	     C only lauching, however generally, the usc DSL should be
	     preferred. Furthermore, an `usc` compiler that can
	     automatically and safely generate the code below is
	     planned. If interested, please ask on the mailing list.

This example is an extension of the example ``c-only.c``. It will be
clear that using the above DSL based method is far easier, but if for
some reason we want to eliminate the dependency from *lua*, this
example show that is possible, even if a little burdensome.

First of all, we need to make a package to enable the building. This can
be done looking at the structure of the rest of packages.

we will create a folder called *platform_launch* that contains the
following files:

- ``main.c``
- ``Makefile.am``
- ``configure.am``


Setup the build system starting from the build part
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

*configure.ac*

.. literalinclude:: ../../examples/platform/platform_launch/configure.ac
   :language: bash


*Makefile.am*

.. literalinclude:: ../../examples/platform/platform_launch/Makefile.am
   :language: make

Here, we specify that the name of the executable is *platform_main* It
might be possible that, if some custom types are used in the
configuration, but are not installed, they must be added to the
``CFLAGS``:

.. code:: make

   platform_main_CFLAGS = -I${top_srcdir}/libubx -I path/to/other/headers @UBX_CFLAGS@

In order to compile, we will use the same commands as before (we do
not need to install).

.. code:: bash

   autoreconf --install
   ./configure
   make

The program
-----------

The main follows the same structure of the ``.usc`` file.

Logging
~~~~~~~

Microblx uses realtime safe functions for logging. For logging from
the scope of a block the functions ``ubx_info``, ``ubx_info``, *etc*
are used. In the main we have to use the functions, ``ubx_log``,
*e.g.*

.. code:: c

   ubx_log(UBX_LOGLEVEL_ERR, &ni, __func__,  "failed to init control1");

More info on logging can be found in the :ref:`real-time-logging`.

Libraries
~~~~~~~~~

It starts with some includes (structs that are needed in
configuration) and loading of the libraries

.. literalinclude:: ../../examples/platform/platform_launch/main.c
   :language: c
   :lines: 1-36	      


Block instantiation
~~~~~~~~~~~~~~~~~~~

Then we instantiate blocks (code for only one, for sake of brevity):

.. literalinclude:: ../../examples/platform/platform_launch/main.c
   :language: c
   :lines: 39-42	      

Property configuration
~~~~~~~~~~~~~~~~~~~~~~

Now we have the more tedious part, that is the configuration. It is
necessary to allocate memory with the function ``ubx_data_resize``,
that takes as argument the data pointer, and the new length.

String property
^^^^^^^^^^^^^^^

.. literalinclude:: ../../examples/platform/platform_launch/main.c
   :language: c
   :lines: 66-71

The string can be defined literally, as a ``static const char[]`` or
using a ``#define``.

Double property
^^^^^^^^^^^^^^^

.. literalinclude:: ../../examples/platform/platform_launch/main.c
   :language: c
   :lines: 83-85

In this case, memory allocation is done for a scalar (i.e. size 1)
. The second line says: consider ``d->data`` as a pointer to double,
and assign to the pointed memory area the value ``0.12``.

Fixed size array of double
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. literalinclude:: ../../examples/platform/platform_launch/main.c
   :language: c
   :lines: 87-90

Same as before, but being a vector, of two elements, the memory
allocation is changed accordingly, and data writings needs the index.

Structure property
^^^^^^^^^^^^^^^^^^

To assign the values to the structure, one option is to make/allocate
a local instance of the structure, and then copy it.

.. literalinclude:: ../../examples/platform/platform_launch/main.c
   :language: c
   :lines: 93-97

Alternatively, you could create and assing a structure:

.. code:: c

   struct ptrig_period p;
   p.sec=1;
   p.usec=14;
   d=ubx_config_get_data(ptrig1, "period");
   ubx_data_resize(d, 1);
   *((struct ptrig_period*)d->data)=p;


Array of structures:
^^^^^^^^^^^^^^^^^^^^

It combines what we saw for arrays and structures. In the case of the
trigger block, we have to configure the order of blocks,

.. literalinclude:: ../../examples/platform/platform_launch/main.c
   :language: c
   :lines: 117-123


Port connection
~~~~~~~~~~~~~~~

To connect we have first to retrieve the ports, and then connect to an a
**iblock**, the fifos. In the following, we have two inputs and two
output ports, that are connected via two fifos:

.. literalinclude:: ../../examples/platform/platform_launch/main.c
   :language: c
   :lines: 148-156

	   
Init and Start the blocks
~~~~~~~~~~~~~~~~~~~~~~~~~

Lastly, we need to init and start all the blocks. For example, for the
``control1`` iblock:

.. literalinclude:: ../../examples/platform/platform_launch/main.c
   :language: c
   :lines: 169-172,204-207

The same applies to all other blocks.

Once all the block are running, the ``trigger`` block will call all
the blocks in the given order, so long the main does not terminate. To
prevent the main process to terminate, we can insert either a blocking
call to terminal input:

.. code:: c

   getchar();

or using the *signal.h* library, wait until *CTRL+C* is pressed:

.. literalinclude:: ../../examples/platform/platform_launch/main.c
   :language: c
   :lines: 204,221

Note that we have to link against pthread library, so the
*Makefile.am* has to be modified accordingly:

.. code:: make

   platform_main_LDFLAGS = -module -avoid-version -shared -export-dynamic  @UBX_LIBS@ -ldl -lpthread


Next steps
----------

Some suggestions for next steps:

- it can be necessary to make the *array size* of data sent and
  received via ports configurable. Checkout the `saturation block
  <https://github.com/kmarkus/microblx/blob/master/std_blocks/saturation/saturation.c>`_
  for a simple example.
  
- sometimes a block shall support multiple types. This can be done at

  * *compile time*: (example `ramp block block
    <https://github.com/kmarkus/microblx/blob/master/std_blocks/ramp/ramp.c>`_)
      
  * *run-time*: (examples: most iblocks, e.g. `lfds_cyclic
    <https://github.com/kmarkus/microblx/blob/master/std_blocks/lfds_buffers/lfds_cyclic.c>`_)
      
