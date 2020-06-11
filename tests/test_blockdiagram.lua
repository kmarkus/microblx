local luaunit=require("luaunit")
local utils=require("utils")
local ubx=require("ubx")
local bd = require("blockdiagram")

ubx.color=false

local assert_not_nil = luaunit.assert_not_nil
local assert_equals = luaunit.assert_equals
local assert_error_msg_equals = luaunit.assert_error_msg_equals

local NUM_BLOCKS = 10

local luablock = [[
local ubx=require "ubx"
local ffi=require("ffi")

function init(block)
   assert(ubx.port_add(block, "in", "test in-port", 0, "uint32_t", 1, nil, 0))
   return true
end

function cleanup(block)
   local b=ffi.cast("ubx_block_t*", block)
   assert(ubx.port_rm(block, "in"))
   ubx.info(b.nd, b.name, "cleanup, removed port 'in'")
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
      res[#res+1] = { name="uint32_ramp"..tostring(i), config = { slope=1} }
   end

   for i=1,NUM_BLOCKS do
      res[#res+1] = { name="sink"..tostring(i), config = { lua_str=luablock}}
   end
   return res
end

--- Test launching a simple composition
local sys1 = bd.system {
   imports = { "stdtypes", "ramp_uint32", "lfds_cyclic", "luablock" },
   blocks = sys1_gen_blocks(),
   connections = sys1_gen_connections(),
   configurations = sys1_gen_configurations(),
}

function test_launch()
   local nd=sys1:launch{nodename="sys1", nostart=true }
   assert_not_nil(nd)
   ubx.node_cleanup(nd)
end


--- Test resolving a node config
local sys_ndcfg_res = bd.system {
   imports = { "stdtypes", "ramp_int32" },
   blocks = { { name = "r1", type = "ramp_int32" } },
   node_configurations = { foo = { type="int32_t", config = 33 } },
   configurations = { { name = "r1", config = { start = 0, slope="&foo" } } }
}

function test_resolve_ndcfg()
   assert_not_nil(sys_ndcfg_res:launch{ nodename="test_resolve_ndcfg", nostart=true })
end

--- Test that an invalid node config is caught
local sys_invalid_ndcfg = bd.system {
   imports = { "stdtypes", "ramp_int32" },
   blocks = { { name = "r1", type = "ramp_int32" } },
   node_configurations = { foo = { type="int32_t", config = 33 } },
   configurations = { { name = "r1", config = { start = 0, slope="&fooX" } } }
}

function test_resolve_ndcfg_invalid()
   local numerr, res = bd.system.validate(sys_invalid_ndcfg, false)
   assert_equals(numerr, 1)
   assert_equals(utils.strip_ansi(res.msgs[1]),
		 "err @ : unable to resolve node config &fooX")
end

--- Test resolving of #block
function test_resolve_block_hash()

   local sys = bd.system {
      imports = { "stdtypes", "ramp_int32", "trig" },
      blocks = {
	 { name = "r1", type = "ramp_int32" },
	 { name = "t1", type = "std_triggers/trig" }
      },

      configurations = {
	 { name = "r1", config = { start=0, slope=1 } },
	 { name="t1",
	    config = {
	       trig_blocks = {
		  { b="#r1", num_steps=1, measure=0 } } } } } }

   assert_not_nil(sys:launch{ nodename="test_resolve_block_hash", nostart=true })
end

--- Test resolving of #block
function test_resolve_block_hash_invalid()

   local sys = bd.system {
      imports = { "stdtypes", "ramp_int32", "trig" },
      blocks = {
	 { name = "r1", type = "ramp_int32" },
	 { name = "t1", type = "std_triggers/trig" }
      },

      configurations = {
	 { name = "r1", config = { start=0, slope=1 } },
	 { name="t1",
	    config = {
	       trig_blocks = {
		  { b="#g1", num_steps=1, measure=0 } } } } } }

   local numerr, res = bd.system.validate(sys, false)
   assert_equals(numerr, 1)
   assert_equals(utils.strip_ansi(res.msgs[1]),
		 "err @ : unable to resolve block ref #g1")
end

os.exit( luaunit.LuaUnit.run() )
