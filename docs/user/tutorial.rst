.. role:: raw-latex(raw)
   :format: latex
..

Tutorial: close loop control of a robotic platform
==================================================

Goal
----

This is a walk-trough that shows how to:

1. write, compile and install two blocks:

   -  the plant (a two DoF robot that accepts velocity as input, and
      give the relative position), and
   -  The controller, that given as a property the set-point and the
      gain, computes the desired velocity to be set to the robot.

2. instantiate the blocks via the ``ubx-launch`` tool, and
3. instantiate the blocks with a c-written program.

All the files can be found in the examples/platform folder.

Introductory steps
------------------

First of all, we need need to define the interface of and between our
two components.

The plant and controller has two ports, that exchange position and
velocity, each of dimension two, and some properties (initial position
and velocity limits for the plant, gain and setpoint for the
controller). These properties are described it two lua files:

The plant, “platform_2dof.lua”

.. code:: lua

   return block
   {
         name="platform_2dof",
         license="MIT",
         meta_data="",
         port_cache=true,

         configurations= {
        { name="joint_velocity_limits", type_name="double", len=2 },
        { name="initial_position", type_name="double", len=2 },
         },

         ports = {
        { name="pos", out_type_name="double", out_data_len=2, doc="measured position [m]" },
        { name="desired_vel", in_type_name="double", in_data_len=2, doc="desired velocity [m/s]" }
         },

         operations = { start=true, stop=true, step=true }
   }

The controller, “platform_2dof_control.lua”

.. code:: lua

   return block
   {
         name="platform_2dof_control",
         license="MIT",
         meta_data="",
         port_cache=true,

         configurations= {
        { name="gain", type_name="double" },
        { name="target_pos", type_name="double" ,len=2 },
         },

         ports = {
        { name="measured_pos", in_type_name="double", in_data_len=2, doc="measured position [m]" },
        { name="commanded_vel", out_type_name="double", out_data_len=2, doc="desired velocity [m/s]" },
         },

         operations = { start=true, stop=true, step=true }
   }

Let us have these file in a folder (*e.g.* microblx_tutorial). from
these file we can generate the two blocks using the the following bash
commands

.. code:: bash

   cd microblx_tutorial
   ubx-genblock -c platform_2dof.lua -d platform_2dof
   ubx-genblock -c platform_2dof_control.lua -d platform_2dof_control

Each command generates a folder with the name specified after the -d
with five files. For the plant, we will have:

-  configure.ac
-  Makefile.am
-  platform_2dof.h
-  platform_2dof.c
-  platform_2dof.usc

The only files we will modify are **platform_2dof/platform_2dof.c** and
**platform_2dof_control/platform_2dof_control.c**. ## Code of the blocks
The file that is auto-generated gives already some hints on how to
approach the programming.

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
           ERR("platform_2dof: failed to alloc memory");
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
     ubx_info(b, "platform_2dof start");
       int ret = 0;
       return ret;
   }

   /* stop */
   void platform_2dof_stop(ubx_block_t *b)
   {
       /* struct platform_2dof_info *inf = (struct platform_2dof_info*) b->private_data; */
     ubx_info(b, "platform_2dof stop");
   }

   /* cleanup */
   void platform_2dof_cleanup(ubx_block_t *b)
   {
       /* struct platform_2dof_info *inf = (struct platform_2dof_info*) b->private_data; */
     ubx_info(b, "platform_2dof cleanup");
       free(b->private_data);
   }

   /* step */
   void platform_2dof_step(ubx_block_t *b)
   {
       /* struct platform_2dof_info *inf = (struct platform_2dof_info*) b->private_data; */
       ubx_info(b, "platform_2dof step");
   }

We will need then to insert the code indicated by the comments. ### Step
1: insert the state of the robot, and other helper functions At the
beginning of the file we insert the following code, to save the state of
the robot:

.. code:: c

   struct robot_state {
     double pos[2];
     double vel[2];
     double vel_limit[2];
   };
   double sign(double x)
   {
     if(x > 0) return 1;
     if(x < 0) return -1;
     return 0;
   }

   struct platform_2dof_info
   {
     /* add custom block local data here */
     struct robot_state r_state;
     struct ubx_timespec last_time;
     /* this is to have fast access to ports for reading and writing, without
            * needing a hash table lookup */
     struct platform_2dof_port_cache ports;
   };

| The “last_time” variable is needed to compute the time passed between
  two calls of
| the “platform_2dof_step” function.

Step 2: Initialization and Start functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This is the function called when the block is initialized; it allocates
memory for the info structure, caches the ports, and initializes the
state given the configuration values (these values are given in the
“.usc” or main application file).

.. code:: c

   int platform_2dof_init(ubx_block_t *b)
   {
     int ret = -1;
     long len;
     struct platform_2dof_info *inf;
     double *pos_vec;

     /* allocate memory for the block local state */
     if ((inf = calloc(1, sizeof(struct platform_2dof_info)))==NULL) {
         ubx_err(b,"platform_2dof: failed to alloc memory");
         ret=EOUTOFMEM;
         goto out;
       }
     b->private_data=inf;
     update_port_cache(b, &inf->ports);

     //read configuration - initial position
     if ((len = ubx_config_get_data_ptr(b, "initial_position",(void **)&pos_vec)) < 0) {
         ubx_err(b,"platform_2dof: failed to load initial_position");
         goto out;
       }
     inf->r_state.pos[0]=pos_vec[0];
     inf->r_state.pos[1]=pos_vec[1];
     if ((len = ubx_config_get_data_ptr(b, "joint_velocity_limits",(void **)&pos_vec)) < 0) {
         ubx_err(b,"platform_2dof: failed to load joint_velocity_limits");
         goto out;
       }
     //read configuration - max velocity
     inf->r_state.vel_limit[0]=pos_vec[0];
     inf->r_state.vel_limit[1]=pos_vec[1];

     inf->r_state.vel[0]=inf->r_state.vel[1]=0.0;
     ret=0;
   out:
     return ret;
   }

The function
``ubx_config_get_data_ptr(ubx_block_t *b, const char *name, void **ptr)``
returns in the pointer passed by reference the address of the required
configuration. In this case the function will return “2”, success, the
length of the data or “-1”, failure.

For the start function, we only need to initialize the internal timer

.. code:: c

   int platform_2dof_start(ubx_block_t *b)
   {
     struct platform_2dof_info *inf = (struct platform_2dof_info*) b->private_data;
     ubx_info(b, "platform_2dof start");
     ubx_gettime(&(inf->last_time));
     int ret = 0;
     return ret;
   }

Step 3: Step function
~~~~~~~~~~~~~~~~~~~~~

In the step function, we compute the time since last iteration, read the
commanded velocity, integrate to position, and then write position.

.. code:: c

   void platform_2dof_step(ubx_block_t *b)
   {
     int32_t ret;
     double velocity[2];
     struct ubx_timespec current_time, difference;
     struct platform_2dof_info *inf = (struct platform_2dof_info*) b->private_data;
     //compute time from last call
     ubx_gettime(&current_time);
     ubx_ts_sub(&current_time,&(inf->last_time),&difference);
     inf->last_time=current_time;
     double time_passed= ubx_ts_to_double(&difference);

     //read velocity from port
     ret = read_desired_vel_2(inf->ports.desired_vel, &velocity);
     if (ret<=0){ //nodata
         velocity[0]=velocity[1]=0.0;
         ubx_err(b,"no velocity data");
       }

     for (int i=0;i<2;i++){// saturate and integrate velocity
         velocity[i]=fabs(velocity[i])> inf->r_state.vel_limit[i]? sign(velocity[i])*(inf->r_state.vel_limit[i]):velocity[i];
         inf->r_state.pos[i]+=velocity[i]*time_passed;}
     //write position in the port
     write_pos_2(inf->ports.pos,&(inf->r_state.pos));

   }

In case there is no value in the port ,an error is signaled, and nominal
velocity is set to zero. This will always happens in the first
interaction, since the controller did step yet, thus no velocity command
is available.

Step 4: Stop and clean-up functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

These functions are OK as they are generated, since the only thing we
want to take care of is that memory is freed.

Final listings of the block
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The plant is, *mutatis mutandis*, built following the same rationale,
and will be not detailed here. the final code of the plant and the
controller can be retrieved here **TODO add link to the code**

building of the blocks
~~~~~~~~~~~~~~~~~~~~~~

In order to build and install the blocks, you must execute the following
bash commands in each of the two directories:

.. code:: bash

    autoreconf --install
   ./configure
   make
   sudo make install

see also the quickstart about these. ## Deployment via the usc (
microblx system composition) file. The ``ubx-genblock`` commands
generates two sample files to run independently each block. We want to
run and compose them together, and save the results in a logger file.
The composition file **platform_2dof_and_control.usc** is quite self
explanatory: It indicates

-  which libraries are imported,
-  which block (name, type) are created,
-  the configuration values of properties.

The code is the following.

.. code:: lua

   plat_report_conf = [[{
   { blockname='plat1', portname="pos"},
   { blockname='control1', portname="commanded_vel"}
   }]] -- this is a multiline string


   return bd.system
   {
      imports = {
         "stdtypes",
         "ptrig",
         "lfds_cyclic",
         "logger",
         "platform_2dof",
         "platform_2dof_control",
      },

      blocks = {
         { name="plat1", type="platform_2dof" },
         { name="control1", type="platform_2dof_control" },
         { name="logger_time", type="logging/file_logger" },
         { name="fifo_pos", type="lfds_buffers/cyclic" },
         { name="fifo_vel", type="lfds_buffers/cyclic" },
         { name="ptrig1", type="std_triggers/ptrig" },
      },
      connections = {
        { src="plat1.pos", tgt="fifo_pos" },
        { src="fifo_pos",tgt="control1.measured_pos" },
        { src="control1.commanded_vel",tgt="fifo_vel" },
        { src="fifo_vel",  tgt="plat1.desired_vel" },

      },
      configurations = {
         { name="plat1", config = {  initial_position={1.1,1}, joint_velocity_limits={0.5,0.5} } },
         { name="control1", config = {  gain=0.1, target_pos={4.5,4.5} } },
         { name="fifo_pos", config = { type_name="double", data_len=2, buffer_len=1 } },
         { name="fifo_vel", config = { type_name="double", data_len=2, buffer_len=1 } },
         { name="logger_time", config = { filename="/tmp/platform_time.log",
                                             separator=",",
                                             report_conf = timer_report_conf, } },
         { name="ptrig1", config = { period = {sec=1, usec=0 },
                                      sched_policy="SCHED_OTHER",
                                      sched_priority=0,
                                      chain0={ { b="#plat1", num_steps=1, measure=1 },
                            { b="#control1", num_steps=1, measure=1 },
                            { b="#logger_time", num_steps=1, measure=0 }  } } }
      }

   }

It is worth noticing that configuration types can be arrays (*e.g.*
``target_pos``), strings (``file_name`` and ``report_conf``) and
structures (``period``) and vector of structures (``chain0``).
Types for each property should be checked in source files.
Alternatively, the web server can provide insight of the types.

The file is launched with the command

.. code:: bash

   ubx-ilaunch -c platform_2dof_and_control.usc

or

.. code:: bash

   ubx-ilaunch -webif -c platform_2dof_and_control.usc

to enable the *web interface* at `localhost:8888 <localhost:8888>`__ .
In order to visualize the data saved by the logger in the
*:raw-latex:`\tmp`* folder, consider `kst <https://kst-plot.kde.org/>`__
or any other program that can visualize a comma-separated-value file.
###Some considerations about the fifos

First of all, consider that each (iblock) fifo can be connected with
multiple input and multiple output ports. Consider also, that if
multiple out are connected, if one read one data, that data will be
consumed and not available for a second port.

The more common use-case is that each inport has is own fifo. If the
data that is produced by one outport is needed to be read by two oe more
inports, a fifo per inport is connected to the the outport. **If you use
the DSL, this is automatically done, so you do not have to worry to
explicitly instantiate the iblocks. This also happens when adding ports
to the logger**.

**TODO insert picture**

##Deployment via c program This example is an extension of the
*“c-only.c”*. It will be clear that using the above method is far
easier, but in case for some reason we want to eliminate the dependency
from *lua*, this example show that is possible, even if a little
burdensome.

First of all, we need to make a package to enable the building. This can
be done looking at the structure of the rest of packages.

we will create a folder called *platform_launch* that contains the
following files:

-  *main.c*
-  *Makefile.am*
-  *configure.am* ##setup the build system starting from the build part:
   *configure.am*:

::

   m4_define([package_version_major],[0])
   m4_define([package_version_minor],[0])
   m4_define([package_version_micro],[0])

   AC_INIT([platform_launch], [package_version_major.package_version_minor.package_version_micro])
   AM_INIT_AUTOMAKE([foreign -Wall])

   # compilers
   AC_PROG_CC

   PKG_PROG_PKG_CONFIG
   PKG_INSTALLDIR

   AC_CONFIG_HEADERS([config.h])
   AC_CONFIG_MACRO_DIR([m4])

   # Check if the `install` program is present
   AC_PROG_INSTALL

   m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
   LT_INIT(disable-static)

   PKG_CHECK_MODULES(UBX, ubx0 >= 0.6.0)

   PKG_CHECK_VAR([UBX_MODDIR], [ubx0], [UBX_MODDIR])
     AC_MSG_CHECKING([ubx module directory])
     AS_IF([test "x$UBX_MODDIR" = "x"], [
     AC_MSG_FAILURE([Unable to identify ubx module path.])
   ])
   AC_MSG_RESULT([$UBX_MODDIR])

   AC_CONFIG_FILES([Makefile])
   AC_OUTPUT

*Makefile.am*

.. code:: make

   ubxmoddir = ${UBX_MODDIR}

   ACLOCAL_AMFLAGS= -I m4
   ubxmod_PROGRAMS = platform_main
   platform_main_SOURCES = $(top_srcdir)/libubx/ubx.h  main.c
   platform_main_CFLAGS = -I${top_srcdir}/libubx  @UBX_CFLAGS@
   platform_main_LDFLAGS = -module -avoid-version -shared -export-dynamic  @UBX_LIBS@ -ldl

Here, we specify that the name of the executable is *platform_main* It
might be possible that, if some custom types are used in the
configuration, but are not installed, they must be added to the
*CFLAGS*:

.. code:: make

   platform_main_CFLAGS = -I${top_srcdir}/libubx -I path/to/other/headers  @UBX_CFLAGS@

In order to compile, we will use the same commands as before (we do not
need to install).

.. code:: bash

   autoreconf --install
   ./configure
   make

The program
-----------

The main follows the same structure of the .usc file. ### Logging
Microblx uses realtime safe functions for logging. For logging from the
scope of a block the functions ``ubx_info``, ``ubx_info``, *etc* are
used. In the main we have to use the functions, ``ubx_log``, *e.g.*

.. code:: c

   ubx_log(UBX_LOGLEVEL_ERR, &ni, __func__,  "failed to init control1");

More info on logging can be found in the `Using real-time logging
section <manual.md#using-real-time-logging>`__. ### Libraries It start
with some include (struct that are needed in configuration) and loading
the libraries

.. code:: c

   #include <ubx.h>

   #define WEBIF_PORT "8810"
   #define DOUBLE_STR "double"
   #include <ubx/triggee.h>
   #include "ptrig_period.h"
   #define LEN_VEC(a) (sizeof(a)/sizeof(a[0]))
   int main()
   {
     int len, ret=EXIT_FAILURE;
     ubx_node_t ni;
     ubx_block_t *plat1, *control1, *logger1, *ptrig1, *webif, *fifo_vel, *fifo_pos;
     ubx_data_t *d;

     /* initalize the node */
     ubx_node_init(&ni, "platform_and_control");
   const char *modules[8];
   modules[0]= "/usr/local/lib/ubx/0.6/stdtypes.so";
   modules[1]= "/usr/local/lib/ubx/0.6/ptrig.so";
   modules[2]= "/usr/local/lib/ubx/0.6/platform_2dof.so";
   modules[3]= "/usr/local/lib/ubx/0.6/platform_2dof_control.so";
   modules[4]= "/usr/local/lib/ubx/0.6/webif.so";
   modules[5]= "/usr/local/lib/ubx/0.6/logger.so";
   modules[6]= "/usr/local/lib/ubx/0.6/lfds_cyclic.so";
     /* load modules */
   for (int i=0; i<LEN_VEC(modules);i++)
     if(ubx_module_load(&ni, modules[i]) != 0){
         printf("fail to load %s",modules[i]);
         goto out;
       }

Block instantiation
~~~~~~~~~~~~~~~~~~~

Then, we instantiate blocks (code for only one, for sake of brevity):

.. code:: c

   if((plat1 = ubx_block_create(&ni, "platform_2dof", "plat1"))==NULL){
         printf("fail to create platform_2dof");
         goto out;
       }

Property configuration
~~~~~~~~~~~~~~~~~~~~~~

Now we have the more tedious part, that is the configuration. it is
necessary to allocate memory with the function ``ubx_data_resize``, that
takes as argument the data pointer, and the new length. #### String
property:

.. code:: c

   d = ubx_config_get_data(webif, "port");
   len = strlen(WEBIF_PORT)+1;
   /* resize the char array as necessary and copy the port string */
   ubx_data_resize(d, len);
   strncpy((char *)d->data, WEBIF_PORT, len);

Here the sting is declared with a ``#define``, it can be written
directly, or with a variable, *e.g.*
``char filename[]="/tmp/platform_time.log"``;

Double property:
^^^^^^^^^^^^^^^^

.. code:: c

   d = ubx_config_get_data(control1, "gain");
   ubx_data_resize(d, 1);
   *((double*)d->data)=0.12;

In this case, memory allocation is done for a scalar (i.e. size 1) . The
second line says: consider ``d->data`` as a pointer to double, and
assign to the pointed memory area the value ``0.12``. #### Fixed size
array of double:

.. code:: c

   d = ubx_config_get_data(control1, "target_pos");
   ubx_data_resize(d, 2);
   ((double*)d->data)[0]=4.5;
   ((double*)d->data)[1]=4.52;

Same as before, but being a vector, of two elements, the memory
allocation is changed accordingly, and data writings needs the index.
#### Structure property: To assign the values to the structure, one
option is to make/allocate a local instance of the structure, and then
copy it.

.. code:: c

   struct ptrig_period p;
   p.sec=1;
   p.usec=14;
   d=ubx_config_get_data(ptrig1, "period");
   ubx_data_resize(d, 1);
   *((struct ptrig_period*)d->data)=p;

In alternative, we can directly work on the fields of the structure

.. code:: c

   d=ubx_config_get_data(ptrig1, "period");
   ubx_data_resize(d, 1);
   ((struct ptrig_period*)d->data)->sec=1;
   ((struct ptrig_period*)d->data)->usec=14;

Array of structures:
^^^^^^^^^^^^^^^^^^^^

It combines what we saw for arrays and structures. In the case of the
trigger block, we have to configure the order of blocks,

.. code:: c

   d=ubx_config_get_data(ptrig1, "chain0");
   len= 3;
   ubx_data_resize(d, len);
   printf("data size trig blocks: %li\n",d->type->size);
   ((struct ubx_triggee*)d->data)[0].b = plat1;//ubx_block_get(&ni, "plat1")
   ((struct ubx_triggee*)d->data)[1].b = control1;
   ((struct ubx_triggee*)d->data)[2].b = logger1;

Port connection
~~~~~~~~~~~~~~~

To connect we have first to retrieve the ports, and then connect to an a
**iblock**, the fifos. In the following, we have two inputs and two
output ports, that are connected via two fifos:

.. code:: c

   ubx_port_t* plat1_pos=ubx_port_get(plat1,"pos");
   ubx_port_t* control1_measured_pos=ubx_port_get(control1,"measured_pos");
   ubx_port_t* control1_commanded_vel=ubx_port_get(control1,"commanded_vel");
   ubx_port_t* plat1_desired_vel=ubx_port_get(plat1,"desired_vel");

   ubx_port_connect_out(plat1_pos,fifo_pos);
   ubx_port_connect_in(control1_measured_pos,fifo_pos);
   ubx_port_connect_out(control1_commanded_vel,fifo_vel);
   ubx_port_connect_in(plat1_desired_vel,fifo_vel);

Init and Start the blocks
~~~~~~~~~~~~~~~~~~~~~~~~~

Lastly, we need to init and start all the blocks. For example, for the
``control1`` iblock:

.. code:: c

   if(ubx_block_init(control1) != 0) {
           ubx_log(UBX_LOGLEVEL_ERR, &ni, __func__,  "failed to init control1");
           goto out;
   }
   if(ubx_block_start(control1) != 0) {
       ubx_log(UBX_LOGLEVEL_ERR, &ni, __func__,  "failed to start control1");
       goto out;
   }

The same applies for all the block.

Once all the block are running, the ``trigger`` block will call all the
blocks in the given order, so long the main does not terminate. To
prevent the main process to terminate, we can insert either a blocking
call to terminal input:

.. code:: c

   getchar();

or using the *signal.h* library, wait until *CTRL+C* is pressed:

.. code:: c

   sigset_t set;
   int sig;

   sigemptyset(&set);
   sigaddset(&set, SIGINT);
   pthread_sigmask(SIG_BLOCK, &set, NULL);
   sigwait(&set, &sig);

Note that we have to link against pthread library, so the *Makefile.am*
has to be modified accordingly:

.. code:: make

   platform_main_LDFLAGS = -module -avoid-version -shared -export-dynamic  @UBX_LIBS@ -ldl -lpthread
