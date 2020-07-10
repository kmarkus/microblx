Launching a microblx app from C
===============================

`c-launch.c` is a tiny example to illustrate how to startup a microblx
application without Lua in plain C. It will launch a single webif
block. It can be compiled as follows:

```bash
$ gcc c-launch.c -o c-launch -lubx
```

Run it

```bash
$ ./c-launch 
loaded request_handler()
started system,webif @ http://localhost:8810
```

Browse to the printed link to check it out. `Ctrl-C` to shutdown.
