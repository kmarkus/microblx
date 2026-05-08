Launching a microblx app from C
===============================

`c-launch.c` is a tiny example to illustrate how to startup a microblx
application without Lua in plain C. It loads the `rand_double` block,
initialises and starts it, then waits for a SIGINT or an optional
timeout argument.

```bash
$ gcc c-launch.c -o c-launch -lubx
```

Run it for 1 second:

```bash
$ ./c-launch 1
started system
shutting down
```
