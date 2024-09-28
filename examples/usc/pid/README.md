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

**Real-time version**

To run this this version addition priviledges are required. The
easiest way to add these is by granting the luajit binary the
`cap_sys_nice` capability (beware that the following will grant this
to all potential luajit users):

```sh
sudo setcap cap_sys_nice+ep `which luajit`
```

After this, the demo can be started as above but by merging the
`ptrig_rt.usc` usc file:

```sh
$ ubx-launch -c pid_test.usc,ptrig_rt.usc
merging ptrig_rt.usc into pid_test.usc
core_prefix: /usr/local
prefixes:    /usr, /usr/local
```

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
