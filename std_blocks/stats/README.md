# ubx/stats

Running statistics of a numeric signal: `min`, `max`, `mean`, `std`
(population standard deviation, Welford's algorithm) and `cnt`, output
as `struct ubx_stat`. The input type is configured at runtime via
`type`.

## Configuration

| config              | type     | description                                                       |
|---------------------|----------|-------------------------------------------------------------------|
| `type`              | `char`   | ubx numeric type name of the input (mandatory)                    |
| `data_len`          | `long`   | vector length; statistics are kept per element (default 1)        |
| `stats_output_rate` | `double` | min seconds between `stats` outputs (0/unset: every step)         |
| `skip_first`        | `long`   | discard the first N samples, e.g. startup transients (default 0)  |
| `loglevel`          | `int`    | optional log level                                                |

Supported `type` values are `int32_t`, `uint32_t`, `int64_t`,
`uint64_t`, `float` and `double`.

## Ports

| port    | direction | type                          | description                |
|---------|-----------|-------------------------------|----------------------------|
| `in`    | in        | `<type>`, created at init     | input signal               |
| `stats` | out       | `struct ubx_stat[data_len]`   | statistics per element     |

```c
struct ubx_stat {
	unsigned long cnt;   /* number of samples */
	double min;
	double max;
	double mean;
	double std;          /* population standard deviation */
};
```

## Behaviour

- statistics are updated every step; `stats_output_rate` only throttles
  the output, like trig/ptrig `tstats_output_rate`.
- statistics persist across stop/start and are cleared on (re-)init.
  They are logged at `info` level on stop.
