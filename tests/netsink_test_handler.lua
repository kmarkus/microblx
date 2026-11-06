-- netsink handler of test_netsink.lua: two buffers, one of static data
local ffi = require("ffi")

local extra = ffi.new("int32_t[2]", 11, 22)

return function(data)
   return { { "struct ubx_tstat", data, 1 }, { "int32_t", extra, 2 } }
end
