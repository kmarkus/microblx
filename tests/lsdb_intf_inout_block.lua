-- Minimal luablock test fixture exposing a single in-out port.
--
-- A port is "in-out" when it carries both an in_type and an out_type.
-- This fixture is used by test_lsdb_intf.lua to verify that lsdb-intf's
-- GetBlockInfo correctly reports such ports.
local ubx = require("ubx")
local ffi = require("ffi")

function init(block)
   block = ffi.cast("ubx_block_t*", block)
   -- port_add(block, name, doc, attrs, in_type, in_len, out_type, out_len)
   assert(ubx.port_add(block, "io", "minimal in-out port", 0,
		       "int32_t", 1, "int32_t", 1))
   return true
end

-- echo: copy whatever was written to the in side back out the out side
function step(block)
   block = ffi.cast("ubx_block_t*", block)
   local p = ubx.port_get(block, "io")
   local len, data = ubx.port_read(p)
   if len > 0 then ubx.port_write(p, data) end
end

function cleanup(block)
   ubx.port_rm(block, "io")
end
