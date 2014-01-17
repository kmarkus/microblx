Trajectory Generator based on Reflexxes Motion Library
======================================================

Compilation
-----------

To be used in a ubx block, the library must be compile using the flag
"-fPIC". E.g. for Linux, edit the Linux/Makefile.global and append
"-fPIC" to the lines DEBUG_CC and RELEASE_CC.

To compile the block, create a link "reflexxes" to the source
directory containing the compiled library and run make.

Testing
-------

There sample deployment file `rml_test.usc` will launch a composition
for testing the block. Start all block (the ptrig block last). Then
step the `tgtpos` to send a new random position. Checkout the logfile
`rml_test.log`.

Notes
-----

 - no jerk limits are set, since this feature is not available for the
   LGPL version.

 - Tested with ReflexxesTypeII version 1.2.4.

Todo
----

 - make the number of DOFS a configuration option.
