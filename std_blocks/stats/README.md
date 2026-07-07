stats block
===========

A generic statistics computation block (`ubx/stats`). It accumulates
the scalar values received on its `in` port and computes running
`min`, `max`, `mean` (average), `std` (population standard deviation)
and `cnt` (sample count), emitted as a `struct ubx_stat` on the
`stats` port.

The numeric input type is configurable at runtime via the `type`
config, so a single block works for any of the supported numeric ubx
types (`int32_t`, `uint32_t`, `int64_t`, `uint64_t`, `float`,
`double`). The `in` port is created at init with the configured type.

Mean and standard deviation are computed online with Welford's
algorithm (numerically stable, single pass). The standard deviation is
the *population* standard deviation (variance divided by `cnt`), which
is the RMS-consistent convention used in signal processing and is
well-defined for `cnt == 1`.

Statistics are cumulative: they persist across `stop`/`start` and are
only cleared on (re-)`init`. The accumulated statistics are logged
with `info` loglevel when the block is stopped.

configs
-------

| name              | type   | doc                                                                    |
|-------------------|--------|------------------------------------------------------------------------|
| type              | char   | ubx numeric type name of the input signal (mandatory)                  |
| data_len          | long   | vector length; statistics are kept per element (default 1)             |
| stats_output_rate | double | throttle output on the `stats` port [sec] (0/unset: output every step) |
| loglevel          | int    | block loglevel                                                         |

With `data_len` > 1 the `in` port is a vector and the `stats` port is a
`struct ubx_stat` array of the same length. Element *i* of the output
describes the *i*-th vector element (channel) accumulated independently
across all steps — i.e. each index is treated as its own scalar signal.

`stats_output_rate` works like the trig/ptrig `tstats_output_rate`: it
limits how often the current statistics are written to the `stats`
port to at most once per `stats_output_rate` seconds. The statistics
themselves are still updated on every step. When unset (or `0`), the
stats are emitted on every step.

ports
-----

| name  | dir | type            | doc                        |
|-------|-----|-----------------|----------------------------|
| in    | in  | *type* (config) | input signal to accumulate |
| stats | out | struct ubx_stat | running statistics output  |

struct ubx_stat
---------------

```c
struct ubx_stat {
	unsigned long cnt;   /* number of samples */
	double min;          /* smallest sample */
	double max;          /* largest sample */
	double mean;         /* arithmetic mean */
	double std;          /* population standard deviation */
};
```

All fields are `double` (except `cnt`), independent of the configured
input type, so the type of the `stats` port is fixed.
