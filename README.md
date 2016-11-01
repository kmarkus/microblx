MICROBLX: real-time, embedded, reflective function blocks
=========================================================

What is it?
-----------

Microblx is an lightweight, dynamic, reflective, hard real-time safe
function block framework:

 - Pure C, no external dependencies
 - Lua scripting for system configuration and deployment
 - Standard block and type library
 - Webinterface function block to introspect and control blocks
 - Automatic block stub code generation
 - Generic Lua scriptable function block
 - Dynamic type handling, no code-generation
 - Similar to IEC-61508 and IEC-61499 functions blocks


Dependencies
------------

 - `luajit`, `libluajit-5.1-dev` (>=2.0.0-beta11, for scripting (optional, but recommended)
 - `liblfds`(v6.1.1, for liblfds_cyclic buffer)
 - `clang++` (only necessary for compiling C++ blocks)
 - gcc (v4.6 or newer) or clang
 - only for development: `cproto` to generate C prototype header file


Building and setting up
------------------------

### Building

Building to run locally on a PC.

Before building microblx, liblfds611 needs to be built and installed. There
is a set of patches in the microblx repository to clean up the packaging
of liblfds. Follow the instructions below:

```
$ cd ~
$ git clone https://github.com/liblfds/liblfds6.1.1.git
$ cd liblfds6.1.1
$ git am ../microblx/liblfds/*.patch
$ ./bootstrap
$ ./configure --prefix=/usr
$ make
$ sudo make install
```

Now build microblx

```
$ cd ~/microblx
$ ./bootstrap
$ ./configure --prefix=/usr
$ make
$ sudo make install
```

Some blocks have external dependencies and may fail. Check for a
README in the respective subdirectory.

Running:

luajit /usr/share/lua/5.1/trig_rnd_to_hexdump.lua

Documentation
-------------

 - [Quickstart](/doc/quickstart.md)
 - [User manual](/doc/manual.md)
 - [API Changes](/API_Changes.md)


Mailing List
------------

Feel free to ask questions on the microblx mailing list:

 http://lists.mech.kuleuven.be/mailman/listinfo/microblx


FAQ
---

### I get module XZY not found

Did you source the `env.sh` script or setup your environment
otherwise?

### My script immedately crashes/finishes

This can have several reasons:

* You forgot the `-i` option to `luajit`: in that case the script is
  executed and once completed will immedately exit. The system will be
  shut down / cleaned up rather rougly.

* You ran the wrong Lua executable (e.g. a standard Lua instead of
  `luajit`).

### Real-time priorities

To run with real-time priorities, give the luajit binary
`cap_sys_nice` capabilities, e.g:

```
$ sudo setcap cap_sys_nice+ep /usr/local/bin/luajit-2.0.2
```

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