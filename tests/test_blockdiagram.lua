local luaunit=require("luaunit")
local ffi=require("ffi")
local ubx=require("ubx")
local bd = require("blockdiagram")

ubx.color=false

local assert_not_nil = luaunit.assert_not_nil

local NUM_BLOCKS = 10

local luablock = [[
local ubx=require "ubx"
local ffi=require("ffi")

function init(block)
   assert(ubx.port_add(block, "in", "test in-port", "uint32_t", 1, nil, 0, 0))
   return true
end

function cleanup(block)
   local b=ffi.cast("ubx_block_t*", block)
   assert(ubx.port_rm(block, "in"))
   ubx.info(b.ni, b.name, "cleanup, removed port 'in'")
end
]]

local function sys1_gen_blocks()
   local res = {}
   for i=1,NUM_BLOCKS do
      res[#res+1] = { name="uint32_ramp"..tostring(i),
		      type="ramp_uint32" }
   end

   for i=1,NUM_BLOCKS do
      res[#res+1] = { name="sink"..tostring(i),
		      type = "lua/luablock" }
   end
   return res
end

local function sys1_gen_connections()
   local res = {}
   for i=1,NUM_BLOCKS do
      res[#res+1] = { src="uint32_ramp"..tostring(i)..".out",
		      tgt="sink"..tostring(i)..".in" }
   end
   return res
end

local function sys1_gen_configurations()
   local res = {}
   for i=1,NUM_BLOCKS do
      res[#res+1] = { name="uint32_ramp"..tostring(i), config = {} }
   end

   for i=1,NUM_BLOCKS do
      res[#res+1] = { name="sink"..tostring(i), config = { lua_str=luablock}}
   end
   return res
end

local sys1 = bd.system {
   imports = { "stdtypes", "ramp_uint32", "lfds_cyclic", "luablock" },
   blocks = sys1_gen_blocks(),
   connections = sys1_gen_connections(),
   configurations = sys1_gen_configurations(),
}

function test_launch()
   local ni=sys1:launch{nodename="sys1", nostart=true }
   assert_not_nil(ni)
   ubx.node_cleanup(ni)
end

os.exit( luaunit.LuaUnit.run() )
