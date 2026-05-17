Running the PID example
-----------------------

The example can be run either in a real-time or non real-time mode,
depending on which `ptrig` model is used. It is suggested to run
`ubx-log` in a separate window to be able to see the log messages.

**Non real-time version**

```sh
$ ubx-launch -c pid_test.usc,ptrig_nrt.usc
merging ptrig_nrt.usc into pid_test.usc
core_prefix: /usr/local
prefixes:    /usr, /usr/local
```

**Real-time version (`SCHED_FIFO`) and SCHED_DEADLINE version**

Both require `CAP_SYS_NICE`. Use the `run-pid.sh` helper, which
grants it via `sudo capsh` with ambient capabilities so the
session bus and `-dbus` keep working:

```sh
$ ./run-pid.sh rt        # SCHED_FIFO
$ ./run-pid.sh deadline  # SCHED_DEADLINE
```

Do **not** use `setcap cap_sys_nice+ep` on the luajit binary — file
capabilities set the `AT_SECURE` flag on exec, which causes sd-bus to
ignore session-bus environment variables and breaks `-dbus`.

**SCHED_DEADLINE: CPU affinity and cpusets**

`ptrig_deadline.usc` does not pin the thread by default. A DEADLINE
thread's `cpus_allowed` mask must be a superset of its scheduling root
domain. Without cpuset isolation the only root domain covers all
online CPUs, so setting `affinity` to a CPU subset makes
`sched_setattr` fail with `EPERM`. To pin EDF tasks to a subset,
create a cpuset partition (`cpuset.cpus` + `cpuset.sched_load_balance=0`,
see `cpuset(7)`) first, then add `affinity` to match.

**Examining exported signals**

```sh
$ ubx-mq list
   mq id                     type name         array len  type hash
1  controller_trig_1.tstats  struct ubx_tstat  1          243b40de92698defa93a145ace0616d2
2  controller_ramp_des.out   double            10         e8cd7da078a86726031ad64f35f5a6c0
3  ramp_msr.out              double            10         e8cd7da078a86726031ad64f35f5a6c0
4  controller_pid_1.out      double            10         e8cd7da078a86726031ad64f35f5a6c0
```

```sh
$ ubx-mq read controller_pid_1.out
{821422305.02823,821422305.02823,821422305.02823,821422305.02823,821422305.02823,821422305.02823,821422305.02823,821422305.02823,821422305.02823,821422305.02823}
{821435122.42823,821435122.42823,821435122.42823,821435122.42823,821435122.42823,821435122.42823,821435122.42823,821435122.42823,821435122.42823,821435122.42823}
{821447939.92823,821447939.92823,821447939.92823,821447939.92823,821447939.92823,821447939.92823,821447939.92823,821447939.92823,821447939.92823,821447939.92823}
{821460757.52824,821460757.52824,821460757.52824,821460757.52824,821460757.52824,821460757.52824,821460757.52824,821460757.52824,821460757.52824,821460757.52824}
{821473575.22824,821473575.22824,821473575.22824,821473575.22824,821473575.22824,821473575.22824,821473575.22824,821473575.22824,821473575.22824,821473575.22824}
...
```
