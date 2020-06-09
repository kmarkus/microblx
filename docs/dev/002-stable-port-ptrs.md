Stable port and config pointers
===============================

2020-06-08, mk

The approach of storing ports and configs in dynamic (`realloc`)
sized, `{0}` terminated arrays had the advantage that prototype blocks
and instances can use the same storage scheme. However, the big
disadvantage of this approach is that each time an array is resized
due to addition or removal of an object, all cached (port) pointers to
this entity are invalidated, causing subtle problems when these are
accessed later.

To overcome this, ports and configs shall be stored in double linked
lists instead. This simplifies the code and guarantees stable
pointers. Iterating over all ports becomes as simple as:

```C
ubx_port_t *p;

DL_FOREACH(b->ports, p) {
	write_int(p, 42);
}
```

However, the downside of this change is that prototype definitions now
use a different storage scheme (`{0}` terminated array vs. linked
list). To avoid confusion and subtle problems, a new set of structs
(`ubx_block_type`, `ubx_port_type` and `ubx_config_type`) is added
exclusively for the use-case of static block prototype
definition. When registering a `ubx_block_def` a corresponding runtime
"prototype instance" is created, from which other blocks can be
cloned. This has the following advantages:

- accidentially mixing of arrays and linked list is avoided.

- The size of run-time data structures can be reduced substantially,
  as the members used only for static defintions (e.g. port
  `in_type_name`, `out_type_name`) can be dropped, thus reducing the
  memory footprint.

- prototype style inheritance, i.e. creating a copy from any block,
  remains possible. We can still warn when creating a block by cloning
  from a non-prototype block.


 


