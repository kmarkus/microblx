# ubx/mux and ubx/demux

Composition glue for array-valued signals:

- **`ubx/mux`** concatenates `nin` input ports `in0`, `in1`, ... into
  one vector output port `out`.
- **`ubx/demux`** partitions a vector input port `in` into output
  ports `out0`, `out1`, ...

By default every numbered port is scalar (length 1). The optional
`in_len`/`out_len` configs assign each numbered port a **sub-vector
length** instead, so mux concatenates sub-vectors and demux extracts
them — connecting only some of demux's outputs makes it a
**slice/splice** block (unconnected outputs cost nothing).

Both work with **any registered type** — numeric or struct — since the
data is copied bytewise; the element type is configured at runtime via
the `type` config. Typical use is composing the measurement vector of
an array-valued block (e.g. `ubx/kalman`'s `z` input) from independent
sources, and extracting sub-vectors (e.g. the position part of a state
estimate) the other way.

## ubx/mux

### Configuration

| config     | type   | description                                            |
|------------|--------|--------------------------------------------------------|
| `type`     | `char` | ubx type name of the signal (mandatory)                |
| `nin`      | `long` | number of input ports (mandatory, ≥ 1)                 |
| `in_len`   | `long` | sub-vector length per input: scalar (broadcast) or per-port `[nin]` (default 1) |
| `loglevel` | `int`  | optional log level                                     |

### Ports

| port          | direction | type     | len            | description          |
|---------------|-----------|----------|----------------|----------------------|
| `in0`..`in<nin-1>` | in   | `<type>` | `in_len[i]`    | inputs               |
| `out`         | out       | `<type>` | Σ `in_len`     | concatenated output  |

### Behaviour

Each step reads every input into its slot of the output vector. A
vector is emitted if **at least one** input had new data; slots whose
input had no new data keep their **last** value (zeros before the first
sample). Inputs running at different rates are therefore combined into
a latest-value snapshot.

## ubx/demux

### Configuration

| config     | type   | description                                            |
|------------|--------|--------------------------------------------------------|
| `type`     | `char` | ubx type name of the signal (mandatory)                |
| `nout`     | `long` | number of output ports (mandatory, ≥ 1)                |
| `out_len`  | `long` | sub-vector length per output: scalar (broadcast) or per-port `[nout]` (default 1) |
| `loglevel` | `int`  | optional log level                                     |

### Ports

| port           | direction | type     | len          | description         |
|----------------|-----------|----------|--------------|---------------------|
| `in`           | in        | `<type>` | Σ `out_len`  | vector input        |
| `out0`..`out<nout-1>` | out | `<type>` | `out_len[i]` | sub-vector outputs  |

### Behaviour

Each step reads the vector input (NODATA: the step is skipped) and
writes each sub-vector to its output port.

### Example: slicing a sub-vector

Extract the position part (elements 0-2) of a 6-element `[p, v]`
Kalman state and ignore the velocities:

```lua
{ name="slice", type="ubx/demux" },
-- ...
{ name="slice", config = { type="double", nout=2, out_len={3, 3} } },
-- ...
{ src="kf.x", tgt="slice.in" },
{ src="slice.out0", tgt="plot.pos" },   -- out1 (velocity) left unconnected
```

See [`slice.usc`](slice.usc) for a small runnable demo of this pattern
(`ubx-launch -c slice.usc`, then inspect the exported position slice
with `ubx-mq`).
