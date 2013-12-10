Quickstart
==========

Run the random block example
----------------------------

This (silly) example creates a random number generator block. It's
output is hexdump'ed (using the `hexdump` interaction block) and also
logged using a `file_logger` block.

```sh
$ luajit examples/trig_rnd_to_hexdump.lua
...
```

Browse to http://localhost:8888

Explore:

 1. clicking on the node graph will show the connections
 1. clicking on blocks will show their interface
 1. start the `file_log1` block to enable logging
 1. start the `ptrig1` block to start the system.


Create your first block
-----------------------

### Generate block

```sh
$ tools/ubx_genblock.lua -d std_blocks/myblock -c examples/block_model_example.lua
     generating ...
```

Run `ubx_genblock -h` for full options.

The following files are generated:

 - `Makefile` standard makefile (you can edit this file)
 - `myblock.h` block interface and module registration code (don't edit)
 - `myblock.c` module body (edit and implement functions)
 - `myblock.usc` simple microblx system composition file, see below (can be extended)
 - `types/vector.h` sample type (edit and fill in struct body)
 - `robot_data.h` sample type (edit and fill in struct body)


### Compile the block

```sh
$ make    # could also be run inside std_blocks/myblock
```

### Launch block using ubx_launch

```sh
$ tools/ubx_launch -webif -c std_blocks/myblock/myblock.usc
```

Run `ubx_launch -h` for full options.

Browse to http://localhost:8888


Notes
-----

 - Commands must be run from root of source tree. This will change in
   the future when blocks and libraries are installed.
   
 - Note the "random block example" uses a plain script to launch the
   system, while ubx_launch uses a declarative system model to launch,
   connect and configure blocks.
