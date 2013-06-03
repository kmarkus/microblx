

ffi = require("ffi")

function read_header(fname)
   local f = assert(io.open(fname, "r"))
   local contents = f:read("*all")
   f:close()
   return contents
end

struct_rand_config = ffi.cdef(read_header("struct_rand_config.h"))

x = ffi.new("struct rand_config")
print(x)
print(ffi.typeof(x))
print(x.min)
print("offset min:", ffi.offsetof(x, "min"));\

print("offset max:", ffi.offsetof(x, "max"));
print("offset asd:", ffi.offsetof(x, "asd"));
x.min=3
print(x.min)
print(getmetatable(x))