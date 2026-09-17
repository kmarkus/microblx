# ubx/latch

A single-writer, multi-reader latest-value store. The latch holds the
most recently written value and returns it to every read: a read does
not consume, so the value stays available until the writer replaces it.
Safe across threads, unlike `vstore`.

Use it where a reader is stepped more often than the writer writes and
should see the last value rather than no-data - published state sampled
by an unrelated cycle, or a signal a block only writes when it changes.

Not a default. The return value means "a value exists", not "a new value
arrived", so a reader cannot tell a repeat from a fresh sample. Anything
that must act only on new input, or must record which samples were
absent, needs `lfrb` or `vstore`.

Configs: `type_name` (ubx type name, mandatory) and `data_len` (array
length, default 1). No ports - the only statistic worth having here, a
torn read, is produced by readers, and writing one port from several
reader threads is the unsynchronised sharing this block exists to avoid.

## Behaviour

- Before the first write a read returns 0 (no data), so the reader never
  sees the zeroed buffer as if it were a value.
- The stored length is the length actually written, not `data_len`, so a
  short write does not hand the reader the stale tail.
- Concurrency is a seqlock: the writer is wait-free, and a reader that
  catches a write in progress retries, then reports no-data rather than
  spinning forever.
- Exactly one writer is required. Two interleave the sequence counter
  and can publish a torn value that passes the reader's check. A
  contract, not a checked precondition.

## Example

Select it on the connection; nothing else is needed.

```lua
imports = { "stdtypes", "ptrig", "stats", "latch" },
connections = {
   -- ptrig writes overrun_cnt only when it changes, so a consumer
   -- stepped every cycle finds nothing in between
   { src="ptrig.overrun_cnt", tgt="overruns.in", type="ubx/latch" },
   -- latency_ns is written every cycle: leave it on the default iblock
   { src="ptrig.latency_ns",  tgt="latency.in" },
}
```
