ffi=require "ffi"

ffi.cdef [[
      typedef enum {
	 PORT_DIR_IN  =  1 << 0,
	 PORT_DIR_OUT =  1 << 1
      } port_type;
]]

print(ffi.C.PORT_DIR_IN)
print(ffi.C.PORT_DIR_OUT)
