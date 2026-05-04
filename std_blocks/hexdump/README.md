# ubx/hexdump

Interaction block that prints every written value to stdout as a hex + ASCII dump. Useful for debugging port data.

Not real-time safe (uses `printf`). No configuration, no ports — connect it like any i-block between a source and sink port.
