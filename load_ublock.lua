
local ffi = require("ffi")
-- local bit = require("bit")

ffi.cdef[[
  int __initialize_module(void);
  void __cleanup_module(void);
]]

comp = ffi.load("./random.so")

comp.__initialize_module()
comp.__cleanup_module()