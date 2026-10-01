# Out-of-tree block

Template for a block module built outside the microblx tree:
`oot/scale` (`out = gain * in + offset`) with a custom config type.
Copy, rename and edit.

```sh
mkdir build && cd build
cmake .. && make && sudo make install   # -DUBX_MODDIR=... to install elsewhere
cd .. && ubx-launch -c scale.usc
ubx-mq read scale.out
```

- `CMakeLists.txt`: finds microblx via `pkg-config ubx0`, generates
  the `.hexarr` of each type header with `ubx-tocarr` and installs
  the module to the microblx module dir.
- `scale.c`: the block.
- `types/scale_config.h`: the config type.
- `scale.usc`: ramp -> scale -> mqueue.
