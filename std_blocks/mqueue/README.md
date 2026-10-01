# ubx/mqueue

POSIX message queue i-block for exchanging data with other processes,
e.g. `ubx-mq`. The queue is named `/ubx_<typehash>_<data_len>_<mq_id>`,
so a reader can find the type and length from the name.

## Configuration

| field        | type       | description                                        |
|--------------|------------|----------------------------------------------------|
| `mq_id`      | `char`     | queue base id (required)                           |
| `type_name`  | `char`     | ubx type to transport (required)                   |
| `data_len`   | `long`     | array length per element (default: 1)              |
| `buffer_len` | `long`     | max number of elements in the queue (required)     |
| `blocking`   | `uint32_t` | blocking mode (default: 0)                         |
| `unlink`     | `uint32_t` | `mq_unlink` in cleanup (default: 1)                |

In usc, a connection with only `src` or `tgt` and `type="ubx/mqueue"`
sets all but `blocking` and `unlink`, with `mq_id` set to the peer
`BLOCK.PORT`:

```lua
{ src="thres.event", type="ubx/mqueue" },
```

```sh
ubx-mq read thres.event -p threshold
```
