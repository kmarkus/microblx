Quickstart
==========

Run the random block example
----------------------------

This (silly) example creates a random number generator block. It's
output is hexdump'ed (using the `hexdump` interaction block) and also
logged using a `file_logger` block.

```sh
$ ubx_ilaunch -webif -c /usr/share/microblx/examples/systemmodels/trig_rnd_hexdump.usc
```

Browse to http://localhost:8888

Explore:

 1. clicking on the node graph will show the connections
 1. clicking on blocks will show their interface
 1. start the `file_log1` block to enable logging
 1. start the `ptrig1` block to start the system.


Create your first block
-----------------------

NOTE: the following assumes that microblx has been installed with
`--prefix=/usr/`.

### Generate a block

```sh
$ ubx_genblock -d myblock -c /usr/share/ubx/examples/blockmodels/block_model_example.lua 
    generating myblock/configure.ac
	generating myblock/Makefile.am
	generating myblock/myblock.h
	generating myblock/myblock.c
	generating myblock/myblock.usc
	generating myblock/types/vector.h
	generating myblock/types/robot_data.h
```

Run `ubx_genblock -h` for full options.

The following files are generated:

 - `configure.ac` autoconf input file
 - `Makefile.am` automake input file
 - `myblock.h` block interface and module registration code (don't edit)
 - `myblock.c` module body (edit and implement functions)
 - `myblock.usc` simple microblx system composition file, see below (can be extended)
 - `types/vector.h` sample type (edit and fill in struct body)
 - `robot_data.h` sample type (edit and fill in struct body)


### Compile the block

```sh
$ cd myblock/
$ ./bootstrap
$ ./configure
$ make
$ make install
```

### Launch block using ubx_launch

```sh
$ ubx_ilaunch -webif -c myblock.usc
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
