; -*-Markdown-*-

u5c: fiveC compliant function block composition
===============================================

Important Links
---------------

- http://gcc.gnu.org/onlinedocs/cpp/Macros.html
- http://luajit.org/ext_ffi.html
- http://www.zeromq.org/intro:start

Requirements
------------

* Block model: ports, metadata
* Meta-data: used to define constraints on blocks, periodicity, etc. JSON? or pure lua
* Ports: in/outs (correspond to in-args and out-args + retval)
* Composition of blocks.
* Pure C + Lua. Light, embeddable.

Interaction model: defines what happens on read-write to a port,
i.e. buffering, rendevouz, sending via network. See also Ptolomy.

Elements
--------

* Components:
  define:
   - set of typed in and out ports
   - configuration

Big question
------------

How to deal with the open world of types.
Issues:

- Types safety must be guaranteed. Hash types in some way. I.e. sha256
  the struct def?

- How to define type ports, configuration, etc.

- To which extent can we avoid boxing and explicit serialization. I
  think the latter is mandatory for non-trivial structs. We _must_
  also be able to support protocol buffers, boost serialization etc.

Options:

- Constrain to structs? C++ Objects can be mapped to structs
  (potentially automatically) but that may be non-intuitve. Ok for
  first go.

- Support full type serialization. Necessary eventually.


Ideas
-----

* Auto-generating fblocks from Linux drivers (or interfaces) maybe
  from sysfs?


