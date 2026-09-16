# ubx/vstore

A single-slot value store for a connection whose writer and reader are
stepped by the same trigger, in the same thread. Such a pair needs
neither a queue nor synchronisation: the write always completes before
the read begins, and there is never a second writer or a second reader.
vstore is that case reduced to one buffer, one flag and a memcpy each
way.

Use it where that property holds and the connection cost matters. Per
connection per step `lfrb` performs four atomic read-modify-writes;
vstore also collapses its freeq/usedq/slot/elem indirection into one
allocation, so the payload shares a cache line with the flag guarding it.

Not a default and not a replacement for `lfrb`. There is no ordering
here at all, so a cross-thread pair reads torn data with nothing to warn
you - use `lfrb`, which is correct everywhere, or `latch` when the reader
should see the last value again rather than no-data. Select vstore per
connection, where the same-trigger property has actually been checked.

Configs: `type_name` (ubx type name, mandatory) and `data_len` (array
length, default 1). One port, `overwrites`, counting writes that landed
before the previous value was read. In correct use it stays at zero; a
nonzero count means the precondition does not hold for that connection.
Treat it as a configuration error, not a statistic.

## Behaviour

- With no unread value held a read returns 0, the same contract as
  `lfrb`. Blocks gate on a positive return to tell "a new value arrived"
  from "nothing this step", so a store that always returned its held
  value would silently break them. If you want that, use `latch`.
- The stored length is the length actually written, not `data_len`, so a
  short write does not hand the reader the stale tail.
- No synchronisation whatsoever: the store is a plain memcpy and the
  flag a plain store. Writer and reader are the same thread, so program
  order is all the ordering there is to have.

## Example

Both blocks sit in one chain of one trigger, so the precondition is
visible in the `chain0` below. Move either to a second ptrig and the
connection becomes cross-thread and must go back to `lfrb`.

```lua
imports = { "stdtypes", "ptrig", "ewma", "saturation", "vstore" },
configurations = {
   { name="ptrig", config = {
        period = { sec=0, usec=1000 },
        chain0 = { { b="#filter" }, { b="#limit" } } } },
},
connections = {
   { src="filter.out", tgt="limit.in", type="ubx/vstore" },
}
```
