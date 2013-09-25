MICROBLX: real-time, embedded, reflective function blocks
=========================================================

What is it?
-----------

Microblx is an lightweight, dynamic, reflective, hard real-time safe
function block implementation:

 - Pure C, no external dependencies
 - Lua scripting for system configuration and deployment
 - Webinterface function block to introspect and control blocks
 - generic Lua scriptable function block
 - Dynamic type handling, no code-generation
 - Similar to IEC-61508 and IEC-61499 functions blocks.


Dependencies
------------

 - cproto (generating c prototypes)
 - luajit, libluajit-5.1-dev (>=2.0.0-beta11, for scripting (optional, but recommended)
 - clang (only necessary for compiling C++ blocks)

Building
--------

$ Just run "make".


Documentation
-------------

Non-existant at the moment.

Check for block specific README files.


FAQ
---

To run with real-time priorities, give the luajit binary
`cap_sys_nice` capabilities, e.g:

```
$ sudo setcap cap_sys_nice+ep /usr/local/bin/luajit-2.0.2
```

License
-------

See COPYING. The license is GPLv3 with a linking exception. It boils
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