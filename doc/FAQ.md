Frequently asked questions
==========================

# Real-time priorities

To run with real-time priorities, give the luajit binary
`cap_sys_nice` capabilities, e.g:

```
$ sudo setcap cap_sys_nice+ep /usr/local/bin/luajit-2.0.2
```

# My script immedately crashes/finishes


This can have several reasons:

* You forgot the `-i` option to `luajit`: in that case the script is
  executed and once completed will immedately exit. The system will be
  shut down / cleaned up rather rougly.

* You ran the wrong Lua executable (e.g. a standard Lua instead of
  `luajit`).

