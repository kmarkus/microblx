MICROBLX: real-time, embedded, reflective function blocks
=========================================================

What is it?
-----------

Microblx is an lightweight, dynamic, reflective, hard real-time safe
function block framework. It's Main use-cases are hard real-time
(embedded) control or signal processing systems.

Main features:

 - Pure C, no external dependencies
 - Lua scripting for system configuration and deployment
 - Standard function block and type library
 - Dynamic type handling, no code-generation necessary
 - Webinterface function block to introspect and control blocks
 - Automatic block stub code generation
 - Generic Lua scriptable function block
 - Similar to IEC-61508 and IEC-61499 functions blocks


Dependencies
------------

 - luajit (>=v2.0.0) (apt: `luajit` and `libluajit-5.1-dev`) (not
   strictly required, but recommended)
 - `uutils` Lua modules [github](https://github.com/kmarkus/uutils)
 - `liblfds` lock free data structures (v6.1.1) [github](https://github.com/liblfds/liblfds6.1.1)
 - `lua-unit` (apt: `lua-unit`, src:
   [luaunit](https://github.com/bluebird75/luaunit) (to run the tests)
 - gcc or clang
 - only for development: `cproto` (apt: `cproto`) to generate C prototype header file


Building and setting up
------------------------

### Using yocto

*Note* work-in-progress.

The easiest way to get going is by using the meta-microblx yocto
layer.

### Building manually

Building to run locally on a PC.

Before building microblx, liblfds611 needs to be built and
installed. There is a set of patches in the microblx repository to
clean up the packaging of liblfds. Follow the instructions below:

First build lfds:

```bash
$ git clone https://github.com/liblfds/liblfds6.1.1.git
$ git clone git@github.com:kmarkus/microblx.git
$ cd liblfds6.1.1
$ git am ~/microblx/liblfds/*.patch
$ ./bootstrap
$ ./configure --prefix=/usr
$ make
$ sudo make install
```

Now build microblx:

```bash
$ cd ../microblx
$ ./bootstrap
$ ./configure --prefix=/usr
$ make
$ sudo make install
```

Some blocks have external dependencies and may fail. Check for a
README in the respective subdirectory.

Run a demo example:

```bash
$ ubx_ilaunch -webif -c /usr/share/microblx/examples/systemmodels/trig_rnd_hexdump.usc
```
browser to localhost:8888 to inspect the system.

Documentation
-------------

 - [Quickstart](/doc/quickstart.md)
 - [User manual](/doc/manual.md)
 - [FAQ](/doc/FAQ.md)
 - [API Changes](/API_Changes.md)


Getting help
------------

Please post any problems via the github issue tracker.

License
-------

See COPYING. The license is GPLv2 with a linking exception. It boils
down to the following. Use microblx as you wish in free and
proprietary applications. You can distribute binary function blocks
modules. Only if you make changes to the core (the microblx library),
and distribute these, then you are required to release these under the
conditions of the GPL.


Acknowledgement
---------------

Microblx is considerably inspired by the OROCOS Real-Time
Toolkit. Other influences are the IEC standards covering function
block IEC-61131 and IEC-61499.

This work was supported by the European FP7 projects RoboHow
(FP7-ICT-288533), BRICS (FP7- ICT-231940), Rosetta (FP7-ICT-230902),
Pick-n-Pack (FP7-NMP-311987) and euRobotics (FP7-ICT-248552).
