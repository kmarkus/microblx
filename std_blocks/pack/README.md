# ubx/pack and ubx/unpack

Pass a set of ports from one thread or process to others as one frame:

- **`ubx/pack`** reads its in-ports each step and publishes them as one
  frame to a lock-free broadcast buffer ([lfb](../../liblfb/README.md))
  in POSIX shared memory.
- **`ubx/unpack`** reads the frames of a pack block and writes them to
  its out-ports. Any number of unpack blocks (or non-ubx readers) can
  read one pack block, independently.

Each frame carries a bit per port telling whether that port had new
data in the step that wrote it. unpack writes only those ports, so a
port without new data stays without new data downstream.

Both are hard real-time: `step` does no allocation and no syscall, as
the segment is populated at init. To avoid page faults when first
touching it in `step`, run the process with `--mlockall`.

## Naming: `packid`

A pack and an unpack block are connected by giving them the same
`packid`, which is mandatory for both. Block names don't matter.

| `packid`    | Block in node `n` | Segment    | Meaning                        |
|-------------|-------------------|------------|--------------------------------|
| `pk`        | `up`              | `n.pk`     | relative, same level           |
| `pk`        | `a/up`            | `n.a.pk`   | relative, in subsystem `a`     |
| `pk`        | `a/c/up`          | `n.a.c.pk` | relative, in `a/c`             |
| `c/pk`      | `a/up`            | `n.a.c.pk` | relative, into subsystem `c`   |
| `/sensor`   | any               | `n.sensor` | absolute, in this node         |
| `/a/pk`     | any               | `n.a.pk`   | absolute, in this node         |
| `rec:/pk`   | any               | `rec.pk`   | absolute, in another node      |
| `rec:/a/pk` | any               | `rec.a.pk` | absolute, in another node      |

- **Relative** ids, without a leading `/`, start at the subsystem of the
  block. Pairs in a subsystem use `pk`, and each instance of it gets its
  own segment without any configuration.
- **Absolute** ids start at the root of the node with `/`, or of another
  node with `node:/`. Use them for pairs in different subsystems, like
  `/sensor` on both blocks. A subsystem used twice then needs its `packid`
  set by the parent.
- The **segment** name, `/dev/shm/<node>.<path>`, has dots for the
  slashes. It is what `ubx-pack` shows and takes. So dots are not allowed
  in a `packid`, or in the names of the node and the subsystems of a block
  using one, and neither are empty path elements.

## Rules

A pack block is a writer, an unpack block a reader, of a segment:

- The first to init creates it, the others join it. A reader can only
  create it with `ports`, without them a missing segment is an error. So
  the order of blocks doesn't matter if the reader has them.
- There is one writer at most. A second one fails at init, also if it is
  `ubx-pack write`.
- Writers and readers with `ports` pin the segment:
  - a segment nobody pins is a leftover, e.g. of a crash, and is replaced
  - one somebody pins must match, or init fails: same `id`, `type`, `len`
    and order of `ports`, and the same `buffer_len` if it is set
  - the last one to leave removes it
- A reader without `ports` takes the layout it finds and doesn't pin. So
  it doesn't keep a segment from being replaced. If it was, on its next
  start it switches to the new one if the layout is the same, or fails.
- Who creates it sets `buffer_len`, by default 1. If unset, any is fine.

All of this happens at init, `step` is not involved. Segments which are
no ubx/pack segments are never replaced.

## ubx/pack

### Configuration

| config       | type                   | description                                                  |
|--------------|------------------------|--------------------------------------------------------------|
| `packid`     | `char`                 | the segment, see [Naming](#naming-packid) (mandatory)        |
| `ports`      | `struct ubx_port_spec` | ports to pack, `{ id=<name>, type=<ubx type>, len=<n> }` (mandatory) |
| `buffer_len` | `uint32_t`             | frames a reader may lag behind without losing any (default 1) |

`len` 0 or unset means 1. `type` is any type registered in the node.

### Ports

One in-port per `ports` entry, named `id`, of the given type and length.
The id `overruns` is reserved, for that port of `ubx/unpack`.

### Behaviour

- **init**: creates the in-ports, and creates or joins the segment. The
  frame layout is in its user area, and the ring has `buffer_len + 1`
  slots.
- **step**: reads every in-port into the next slot, sets the bits of
  the ports that had data and commits the frame. A frame is written
  every step, also when no port had data.

## ubx/unpack

### Configuration

| config       | type                   | description                                                  |
|--------------|------------------------|--------------------------------------------------------------|
| `packid`     | `char`                 | the segment, see [Naming](#naming-packid) (mandatory)        |
| `ports`      | `struct ubx_port_spec` | optional: expected ports, the segment is created if missing  |
| `buffer_len` | `uint32_t`             | optional: frames to buffer, as for `ubx/pack`                |

### Ports

| port       | direction | type            | description                                |
|------------|-----------|-----------------|--------------------------------------------|
| `<id>`     | out       | as packed       | one per packed port, same name, type, len  |
| `overruns` | out       | `unsigned long` | frames lost since init, written on change  |

### Behaviour

- **init**: opens or creates the segment, see [Rules](#rules), reads the
  layout and creates the out-ports. Fails if a type is unknown or of a
  different size in this node.
- **start**: without `ports`, if the segment was replaced meanwhile,
  switches to the new one, provided the layout is unchanged; otherwise
  start fails and the block must be re-initialized too. Then positions
  the reader at the newest frame: frames written while stopped are not
  delivered.
- **step**: reads one frame, the oldest unread one, and writes its
  present ports. No new frame: nothing is written. If the reader fell
  `buffer_len + 1` or more frames behind, the lost frames count as one
  overrun and the oldest surviving frame is read instead (with
  `buffer_len` 1: the newest).

Reading one frame per step means a reader stepped at the same rate as
the pack block stays in sync; one that is stepped slower falls behind.

## ubx-pack

List the pack blocks of all processes, and read one with a `ubx/unpack`
block of its own. `read` shows new frames only.

```sh
$ ubx-pack list
$ ubx-pack read mynode.pk           # CSV, one row per frame
$ ubx-pack read mynode.pk -n 100 -p mytypes > out.csv
$ ubx-pack read mynode.pk -f plain  # a Lua table per frame
$ ubx-pack read mynode.pk -f json   # needs the lua module cjson or json
```

Array and struct ports get one column per element or field, e.g.
`arr[2]` or `ts.cnt`. A cell is empty if its port had no new data. Use
`-p` to load the modules of types other than the `stdtypes`. Overruns
go to stderr.

With `-f plain` and `-f json`, each frame is one line with the ports that
had new data, by id: `{count=200,vec={1,2,3}}` as a Lua table, or
`{"count":200,"vec":[1,2,3]}`. `plain` is exact, a value reads back as it
was, except for a 64 bit integer in a struct above 2^53. `json` prints
numbers with 14 significant digits, so a `uint64_t` above 10^14 and most
`double`s are rounded, leaves out nan and inf, and its keys are in no
particular order.

### Replay

`write` publishes a recording to the segment you name, with `read` output
on stdin. A recording starts with a `--ports=` line holding its layout, so
the source doesn't need to run anymore.

```sh
$ ubx-pack read mynode.pk -f plain > rec.txt
$ ubx-pack write replay.pk -r 100 -w 2 < rec.txt
```

- If the segment exists, `write` joins it, so its ports must match the
  recording, and it must have no writer. It works any number of times on
  a segment held by a reader with `ports`, like in the
  [example](#example).
- If it doesn't, `write` creates it, and removes it at the end unless a
  reader with `ports` is still there. Readers need it to exist, and only
  see new frames: start them within the seconds given by `-w`.
- Use `-r` for a file, as frames have no timestamps: without it frames go
  out as fast as they are read, and a slower reader loses most of them.
  `-b` sets `buffer_len`.
- A port without data in a frame also has none in the replay.

## Frame layout

For non-ubx clients in C: `pack_shm.h` (installed to `include/ubx/`, with
`pack_layout.h`) has `ubx_pack_join` and `ubx_pack_open`, which take the
segment name, and `ubx_pack_close`. Frames are then plain
[lfb](../../liblfb/README.md) reads and writes. A writer must use it, as
that keeps a second one out. A frame is

```
uint64_t present[ceil(nports / 64)] | port 0 | port 1 | ...
```

with each port at its 8-byte aligned `offset`.

## Example

[`examples/usc/pack.usc`](../../examples/usc/pack.usc) echoes a counter and
the timing statistics of the trigger around a loop through `ubx-pack`:

```
                                 n.pk
 count.out --> +------+  shm  ............   ubx-pack read n.pk -f plain
 tstats -----> |  pk  | ----> : segment  : ----------------------------.
               +------+       :..........:                             |
                                                     rec.txt or a pipe |
                                 n.echo_up                             |
               +---------+  shm ..........   ubx-pack write n.echo_up  |
               | echo_up | <--- : segment : <--------------------------'
               +---------+      :.........:
                    |
                    | count, tstats
                    v
               +---------+  shm ..........
               | echo_pk | ---> : segment : ---> ubx-pack read n.echo_pk
               +---------+      :.........:
                                 n.echo_pk
```

`echo_up` has `ports`, so it creates its segment, and it stays while the
node runs. No block writes to it, `ubx-pack write` does. All blocks run
in one chain here: to pass data between threads, step the writer in one
and the reader in the other.

```lua
{ name = "pk", config = { packid = "pk", ports = PORTS } },
{ name = "echo_up", config = { packid = "echo_up", ports = PORTS, buffer_len = 16 } },
{ name = "echo_pk", config = { packid = "echo_pk", ports = PORTS } },
```

Start it, in `examples/usc`:

```sh
$ ubx-launch -c pack.usc
```

In a second terminal, see the data, and send it around the loop:

```sh
$ ubx-pack list
   pack id    buffer_len  ports
1  n.echo_pk  1           count:uint64_t tstats:struct ubx_tstat
2  n.echo_up  16          count:uint64_t tstats:struct ubx_tstat
3  n.pk       1           count:uint64_t tstats:struct ubx_tstat
$ ubx-pack read n.pk
$ ubx-pack read n.pk -f plain | ubx-pack write n.echo_up
```

Or with a recording, as many times as you like:

```sh
$ ubx-pack read n.pk -f plain -n 50 > rec.txt
$ ubx-pack write n.echo_up -r 10 < rec.txt
```

In a third terminal, see what came back:

```sh
$ ubx-pack read n.echo_pk
```

The `tstats` cells are empty except for one frame per second.
