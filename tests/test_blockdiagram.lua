local luaunit=require("luaunit")
local utils=require("utils")
local ubx=require("ubx")
local bd = require("blockdiagram")


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
		      type="ubx/ramp_uint32" }
   end

   for i=1,NUM_BLOCKS do
      res[#res+1] = { name="sink"..tostring(i),
		      type = "ubx/luablock" }
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

TestBlockdiagram = {}

local _nd

function TestBlockdiagram:teardown()
   if _nd then ubx.node_rm(_nd) end
   _nd = nil
end

--- Test launching a simple composition
local sys1 = bd.system {
   imports = { "stdtypes", "ramp_uint32", "lfrb", "luablock" },
   blocks = sys1_gen_blocks(),
   connections = sys1_gen_connections(),
   configurations = sys1_gen_configurations(),
}

function TestBlockdiagram:test_launch()
   _nd=sys1:launch{nodename="sys1", nostart=true }
   assert_not_nil(_nd)
end


--- Test resolving a node config
local sys_ndcfg_res = bd.system {
   imports = { "stdtypes", "ramp_int32" },
   blocks = { { name = "r1", type = "ubx/ramp_int32" } },
   node_configurations = { foo = { type="int32_t", config = 33 } },
   configurations = { { name = "r1", config = { start = 0, slope="&foo" } } }
}

function TestBlockdiagram:test_resolve_ndcfg()
   _nd = sys_ndcfg_res:launch{ nodename="test_resolve_ndcfg", nostart=true }
   assert_not_nil(_nd)
end

--- Test that an invalid node config is caught
local sys_invalid_ndcfg = bd.system {
   imports = { "stdtypes", "ramp_int32" },
   blocks = { { name = "r1", type = "ubx/ramp_int32" } },
   node_configurations = { foo = { type="int32_t", config = 33 } },
   configurations = { { name = "r1", config = { start = 0, slope="&fooX" } } }
}

function TestBlockdiagram:test_resolve_ndcfg_invalid()
   local numerr, res = bd.system.validate(sys_invalid_ndcfg, false)
   assert_equals(numerr, 1)
   assert_equals(utils.strip_ansi(res.msgs[1]),
		 "err @ : unable to resolve node config &fooX")
end

--- Test resolving of #block
function TestBlockdiagram:test_resolve_block_hash()

   local sys = bd.system {
      imports = { "stdtypes", "ramp_int32", "trig" },
      blocks = {
	 { name = "r1", type = "ubx/ramp_int32" },
	 { name = "t1", type = "ubx/trig" }
      },

      configurations = {
	 { name = "r1", config = { start=0, slope=1 } },
	 { name="t1",
	    config = {
	       chain0 = {
		  { b="#r1", num_steps=1, measure=0 } } } } } }

   _nd = sys:launch{ nodename="test_resolve_block_hash", nostart=true }
   assert_not_nil(_nd)
end

--- Test resolving of #block
function TestBlockdiagram:test_resolve_block_hash_invalid()

   local sys = bd.system {
      imports = { "stdtypes", "ramp_int32", "trig" },
      blocks = {
	 { name = "r1", type = "ubx/ramp_int32" },
	 { name = "t1", type = "ubx/trig" }
      },

      configurations = {
	 { name = "r1", config = { start=0, slope=1 } },
	 { name="t1",
	    config = {
	       chain0 = {
		  { b="#g1", num_steps=1, measure=0 } } } } } }

   local numerr, res = bd.system.validate(sys, false)
   assert_equals(numerr, 1)
   assert_equals(utils.strip_ansi(res.msgs[1]),
		 "err @ : unable to resolve block ref #g1")
end

--- Test that extern_blocks allows connections to external blocks
function TestBlockdiagram:test_extern_blocks_valid()

   local sys = bd.system {
      imports = { "stdtypes", "ramp_int32", "lfrb" },
      extern_blocks = { "ext_blk" },
      blocks = {
	 { name = "r1", type = "ubx/ramp_int32" },
      },
      connections = {
	 { src = "r1.out", tgt = "ext_blk.in" },
      },
      configurations = {
	 { name = "r1", config = { start=0, slope=1 } },
      },
   }

   local numerr = bd.system.validate(sys, false)
   assert_equals(numerr, 0)
end

--- Test that a connection to an unknown block (not in blocks or extern_blocks) still fails
function TestBlockdiagram:test_extern_blocks_invalid()

   local sys = bd.system {
      imports = { "stdtypes", "ramp_int32", "lfrb" },
      extern_blocks = { "ext_blk" },
      blocks = {
	 { name = "r1", type = "ubx/ramp_int32" },
      },
      connections = {
	 { src = "r1.out", tgt = "typo_blk.in" },
      },
      configurations = {
	 { name = "r1", config = { start=0, slope=1 } },
      },
   }

   local numerr, res = bd.system.validate(sys, false)
   assert(numerr > 0, "expected validation errors for unknown block ref")
end

--- Test launching extern_blocks into an existing node
function TestBlockdiagram:test_extern_blocks_launch()

   -- first create a node with a "core" block
   local core = bd.system {
      imports = { "stdtypes", "random", "lfrb" },
      blocks = {
	 { name = "core_rnd", type = "ubx/random" },
      },
      configurations = {
	 { name = "core_rnd", config = { min_max_config={min=1, max=100} } },
      },
   }

   _nd = core:launch{ nodename="test_extern", nostart=true }
   assert_not_nil(_nd)

   -- now load an auxiliary system that connects to the core block
   local aux = bd.system {
      imports = { "stdtypes", "random", "lfrb" },
      extern_blocks = { "core_rnd" },
      blocks = {
	 { name = "aux_rnd", type = "ubx/random" },
      },
      connections = {
	 { src = "core_rnd.rnd", tgt = "aux_rnd.seed" },
      },
      configurations = {
	 { name = "aux_rnd", config = { min_max_config={min=1, max=100} } },
      },
   }

   aux:launch{ nd = _nd, nostart=true }

   -- verify aux_rnd exists in the node
   local b = ubx.block_get(_nd, "aux_rnd")
   assert_not_nil(b)
end

--- Test load_str with Lua format (as used by lsdb-intf LoadUSCLua)
function TestBlockdiagram:test_load_str_lua()
   _nd = ubx.node_create("test_load_str_lua",
			   { loglevel=7 })
   ubx.load_module(_nd, "stdtypes")
   ubx.load_module(_nd, "ramp_int32")

   local usc_lua = [[
return bd.system {
   imports = { "stdtypes", "ramp_int32" },
   blocks = {
      { name = "r1", type = "ubx/ramp_int32" },
   },
   configurations = {
      { name = "r1", config = { start=0, slope=1 } },
   },
}
]]
   local sys = bd.load_str(usc_lua, 'lua')
   sys:launch{ nd=_nd, nostart=true }

   local b = ubx.block_get(_nd, "r1")
   assert_not_nil(b)
   assert_equals(b:get_block_state(), "inactive")
end

--- Test load_str with JSON format (as used by lsdb-intf LoadUSCJSON)
function TestBlockdiagram:test_load_str_json()
   local has_json = pcall(require, "cjson") or pcall(require, "json")
   if not has_json then
      print("skipping test_load_str_json: no json library")
      return
   end

   _nd = ubx.node_create("test_load_str_json",
			  { loglevel=7 })
   ubx.load_module(_nd, "stdtypes")
   ubx.load_module(_nd, "ramp_int32")

   local usc_json = [[
{
   "imports": [ "stdtypes", "ramp_int32" ],
   "blocks": [
      { "name": "jr1", "type": "ubx/ramp_int32" }
   ],
   "configurations": [
      { "name": "jr1", "config": { "start": 0, "slope": 1 } }
   ]
}
]]
   local sys = bd.load_str(usc_json, 'json')
   sys:launch{ nd=_nd, nostart=true }

   local b = ubx.block_get(_nd, "jr1")
   assert_not_nil(b)
   assert_equals(b:get_block_state(), "inactive")
end

if not _RUNNER then os.exit( luaunit.LuaUnit.run() ) end
