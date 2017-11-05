Trajectory Generator based on Reflexxes Motion Library
======================================================

Compilation
-----------

To be used in a ubx block, the library must be compiled using the
compiler flag `-fPIC`. E.g. for Linux, edit the
`Linux/Makefile.global` and append `-fPIC` to the lines `DEBUG_CC` and
`RELEASE_CC`.

To compile the block, create a softlink `reflexxes` to the source
directory containing the compiled library and run `make`.


Testing
-------

There sample deployment file `rml_test.usc` will launch a composition
for testing the block. Start all blocks (the ptrig block last). Then
step the `tgtpos` block to compute a new random target
position. Checkout the logfile `rml_test.log`.


Notes
-----

 - no jerk limits are set, since this feature is not available for the
   LGPL version.

 - Tested with ReflexxesTypeII version 1.2.4.

Todo
----

 - make the number of DOFS a configuration option.


Links
-----
 - http://www.reflexxes.com/

