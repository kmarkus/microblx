Frequently asked questions
==========================

## fix "error object is not a string"

This is most of the time happens when the `strict` module being loaded
(also indirectly, e.g. via ubx.lua) in a luablock. It is caused by the
C code looking up a non-existing global hook function. Solution:
either define all hooks or disable the strict module for the luablock.

Note that this has been fixed in
be63f6408bd4dff4de14ce447f367498ae778497.


## `blockXY`.so or `liblfds611.so.0`: cannot open shared object file: No such file or directory

Often this means that the location of the shared object file is not in
the library search path. Try adding it to `LD_LIBRARY_PATH`, e.g.

```sh
$ export LD_LIBRARY_PATH=/usr/local/lib/
```

It probably would be better to install stuff in a standard location by
passing something like `--prefix=/usr/` to `./configure`

## Real-time priorities

To run with realtime priorities, give the luajit binary
`cap_sys_nice` capabilities, e.g:

```
$ sudo setcap cap_sys_nice+ep /usr/local/bin/luajit-2.0.2
```

## My script immedately crashes/finishes

This can have several reasons:

* You forgot the `-i` option to `luajit`: in that case the script is
  executed and once completed will immedately exit. The system will be
  shut down / cleaned up rather rougly.

* You ran the wrong Lua executable (e.g. a standard Lua instead of
  `luajit`).

Typically the best way to debug crashes is using gdb and the core dump
file:

```sh
# enable core dumps
$ ulimit -c unlimited
$ gdb luajit
...
(gdb) core-file core
...
(gdb) bt

``


