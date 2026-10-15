-- configs can not be set while the block is active

local lu = require("luaunit")
local ubx = require("ubx")
local ffi = require("ffi")

local assert_equals = lu.assert_equals
local assert_error_msg_contains = lu.assert_error_msg_contains

local nd

TestConfigActive = {}

function TestConfigActive:setup()
   nd = ubx.node_create("test_config_active", { loglevel = ffi.C.UBX_LOGLEVEL_CRIT })
   ubx.load_module(nd, "stdtypes")
   ubx.load_module(nd, "ramp")
end

function TestConfigActive:teardown()
   ubx.node_rm(nd)
   nd = nil
end

function TestConfigActive:TestSetConfig()
   local b = ubx.block_create(nd, "ubx/ramp", "ramp", { type = "double", slope = 1 })

   -- preinit, inactive: ok
   ubx.set_config(b, "slope", 2)
   assert_equals(ubx.block_tostate(b, 'inactive'), 0)
   ubx.set_config(b, "slope", 3)

   -- active: refused, also for loglevel, value unchanged
   assert_equals(ubx.block_tostate(b, 'active'), 0)
   assert_error_msg_contains("is active, stop it first",
			     ubx.set_config, b, "slope", 4)
   assert_error_msg_contains("is active, stop it first",
			     ubx.set_config, b, "loglevel", 7)
   assert_equals(b:c("slope"):tolua(), 3)

   -- stopped again: ok
   assert_equals(ubx.block_tostate(b, 'inactive'), 0)
   ubx.set_config(b, "slope", 5)
   assert_equals(b:c("slope"):tolua(), 5)
end

function TestConfigActive:TestConfigAssign()
   local b = ubx.block_create(nd, "ubx/ramp", "ramp", { type = "double", slope = 1 })
   local d = ubx.data_alloc(nd, "double")
   local c = ubx.config_get(b, "slope")

   assert_equals(ubx.config_assign(c, d), 0)
   assert_equals(ubx.block_tostate(b, 'active'), 0)
   assert_equals(ubx.config_assign(c, d), ffi.C.EWRONG_STATE)
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
