
ffi = require "ffi"

ffi.cdef[[
      struct foo {
	 int x;
	 int y;
      };
]]

ffi.cdef[[
      typedef struct {
	 int z;
	 char a;
      } frubagatschi_t;
]]

frubagatschi_t = ffi.typeof("frubagatschi_t")

print(frubagatschi_t)
print(frubagatschi_t())
