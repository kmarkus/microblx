local luaunit=require("luaunit")
local bd=require("blockdiagram")
local ffi=require("ffi")

local assert_equals = luaunit.assert_equals

local sys1 = bd.system {
   imports = { "stdtypes",
	       "random",
	       "ramp_uint64" },
   blocks = {
      { name="ramp1", type="ramp_uint64" },
      { name="ramp2", type="ramp_uint64" },
      { name="ramp3", type="ramp_uint64" },

      { name="rand1", type="random/random" },
      { name="rand2", type="random/random" },
      { name="rand3", type="random/random" },
   },

   node_configurations={
      ramp_start = { type="uint64_t", config=10 },
      ramp_slope = { type="uint64_t", config=345 },
      rand_conf = { type="struct random_config",
		    config={ min=222, max=333 },
      }
   },

   configurations = {
      { name="ramp1", config = { start="&ramp_start", slope="&ramp_slope" } },
      { name="ramp2", config = { start="&ramp_start", slope="&ramp_slope" } },
      { name="ramp3", config = { start="&ramp_start", slope="&ramp_slope" } },

      { name="rand1", config = { min_max_config="&rand_conf" } },
      { name="rand2", config = { min_max_config="&rand_conf" } },
      { name="rand3", config = { min_max_config="&rand_conf" } },
   },
}

local nd=nil

function setup()
   nd = sys1:launch{ nostart=true, loglevel=ffi.C.UBX_LOGLEVEL_WARN }
end

function teardown()
   sys1:pulldown(nd)
   nd=nil
end

test_scalar = {}
test_scalar.setup = setup
test_scalar.teardown = teardown

function test_scalar.test_initial_node_conf_rampstart()
   for i=1,3 do
      local blkcfg = nd:b("ramp"..i):c("start"):tolua()
      local nodecfg = sys1.node_configurations.ramp_start.config
      assert_equals(blkcfg, nodecfg, "ramp_start: nodecfg != blkconfig")
   end
end

function test_scalar.test_initial_node_conf_rampslope()
   for i=1,3 do
      local blkcfg = nd:b("ramp"..i):c("slope"):tolua()
      assert_equals(blkcfg, 345, "ramp_slope: nodecfg != blkconfig")
   end
end

function test_scalar.test_node_conf_change_rampstart()
   local new_val = 997755
   nd:b("ramp1"):c("start"):set(new_val)
   for i=1,3 do
      local blkcfg = nd:b("ramp"..i):c("start"):tolua()
      assert_equals(blkcfg, new_val, "ramp_start: nodecfg != blkconfig")
   end
end

function test_scalar.test_node_conf_change_rampslope()
   local new_val = 11223344
   nd:b("ramp3"):c("slope"):set(new_val)
   for i=1,3 do
      local blkcfg = nd:b("ramp"..i):c("slope"):tolua()
      assert_equals(blkcfg, new_val, "ramp_slope: nodecfg != blkconfig")
   end
end

test_struct = {}
test_struct.setup = setup
test_struct.teardown = teardown

function test_struct.test_initial_node_conf_struct()
   for i=1,3 do
      local blkcfg = nd:b("rand"..i):c("min_max_config"):tolua()
      local nodecfg = sys1.node_configurations.rand_conf.config
      assert_equals(blkcfg.min, nodecfg.min, "rand cfg: nodecfg != blkconfig")
      assert_equals(blkcfg.max, nodecfg.max, "rand cfg: nodecfg != blkconfig")
   end
end

function test_struct.test_node_conf_change_struct()
   nd:b("rand2"):c("min_max_config"):set{min=29999, max=131313}

   for i=1,3 do
      local blkcfg = nd:b("rand"..i):c("min_max_config"):tolua()
      assert_equals(blkcfg.min, 29999, "rand cfg: nodecfg != blkconfig")
      assert_equals(blkcfg.max, 131313, "rand cfg: nodecfg != blkconfig")
   end
end

test_array = {}
test_array.setup = setup
test_array.teardown = teardown

function test_array.test_array_change()
   nd:b("rand2"):c("min_max_config"):set{
      {min=1, max=11},
      {min=2, max=22},
      {min=3, max=33} }

   for i=1,3 do
      local blkcfg = nd:b("rand"..i):c("min_max_config"):tolua()
      assert_equals(blkcfg[1].min, 1, "rand cfg: nodecfg != blkconfig")
      assert_equals(blkcfg[1].max, 11, "rand cfg: nodecfg != blkconfig")

      assert_equals(blkcfg[2].min, 2, "rand cfg: nodecfg != blkconfig")
      assert_equals(blkcfg[2].max, 22, "rand cfg: nodecfg != blkconfig")

      assert_equals(blkcfg[3].min, 3, "rand cfg: nodecfg != blkconfig")
      assert_equals(blkcfg[3].max, 33, "rand cfg: nodecfg != blkconfig")
   end
end

os.exit( luaunit.LuaUnit.run() )
