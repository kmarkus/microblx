# microblx roadmap

This file sketches the vision of where microblx is heading in terms of
features and development. If you are interested to work on these, it
is recommended to double check on the mailing list whether somebody
else is already doing so.

## Backlog

- POSIX shared memory communication iblocks are available
  - circular buffer (this may be be based off the upcoming `liblfds` version).
  - single sample (e.g. to communicate a state from the ubx application)

- basic arithmetic cblocks like `+` and `*` are available. Type and
  array len must be configurable.

- the `ubx-mq` tool is extended to allow injecting data. It should
  at least support Lua and JSON.

- support for custom pretty printing of ubx types

- usc: a simpler syntax for exporting port values via message queues
  is available (today these need to be instantiated and configured
  separately)

- usc: `connections`: the default of using lock-free cyclic iblocks
  can be overriden.

- automatic schedule calculation: instead of manually specifying
  `trig` schedules, determine

- usc-compiler: a function block composition can be compiled into a
  standalone application. This would be useful for deploying
  application binaries. A first proof of concept was developed here
  [1].

## References

- [1] <https://github.com/haianos/microblx_cmake/blob/dev/ubx_genbin.lua>
