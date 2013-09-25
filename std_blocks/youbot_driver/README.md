The KUKA YouBot microblx driver
===============================

Compiling
---------

1. Clone the SOEM EtherCAT library from here:

```Shell
$ git clone https://github.com/kmarkus/soem1.2.5.git
```

and compile it:

```Shell
$ cd soem1.2.5/src/
$ make
```


2. In this (microblx) directory, create a softlink named `soem` to the
soem1.2.5 directory:

```Shell
$ cd microblx/std_blocks/youbot_driver/
$ ln -s /path/to/soem1.2.5 soem
```


3. Compile the youbot driver block

```Shell
$ make
```

Running
-------

Run the `youbot_test.lua` script.

NOTE: Always calibrate the arm before use! Run help() to get
information about possible options.

The youbot driver requires permission to open raw sockets (capability
`cap_net_raw`) for EtherCAT communication. Furthermore, it is
advisable to run with real-time priorities (capability:
`cap_sys_nice`). To add these, run the following `setcap` command on
the luajit binary:

```Shell
$ sudo setcap cap_sys_nice,cap_net_raw+ep /usr/local/bin/luajit-2.0.2
```


License
-------

LGPL or Modified BSD


History
-------

This driver derived from the Orocos RTT driver code, with substantial
cleanup and rewriting. Nevertheless, a large amount of code has been
copied, hence the original licensing conditions apply.


Acknowledgement
---------------

The research leading to these results has received funding from the
European Community's Seventh Framework Programme (FP7/2007-2013) under
grant agreement no. FP7-ICT-231940-BRICS (Best Practice in Robotics)

