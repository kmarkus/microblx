local luaunit=require("luaunit")
local ffi=require("ffi")
local ubx=require("ubx")
local utils=require("utils")
local bd = require("blockdiagram")

ubx.color=false

local NUM_BLOCKS = 100

local luablock = [[
local ubx=require "ubx"

function init(block)
   ubx.port_add(block, "in", "test in-port", "uint32_t", 1, nil, 0, 0)
   return true
end

function cleanup(block)
   print("stop")
   print("rm'ing", ubx.port_rm(block, "in"))
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
   return sys1:launch{nodename="sys1", loglevel=ffi.C.UBX_LOGLEVEL_WARN }
end

os.exit( luaunit.LuaUnit.run() )
