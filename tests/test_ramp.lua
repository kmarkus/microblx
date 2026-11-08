--
-- Tests for the runtime-typed ramp generator (ubx/ramp), in particular
-- the *native* accumulation and configs (exact integer counting beyond
-- the 2^53 double mantissa limit).
--

local lu = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")
local ffi = require("ffi")

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_WARN

local assert_equals = lu.assert_equals
local assert_not_nil = lu.assert_not_nil
local assert_true = lu.assert_true

local ni

TestRamp = {}

function TestRamp:teardown()
   if ni then ubx.node_rm(ni) end
   ni = nil
end

local function make(cfg, nodename)
   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "ramp" },
      blocks = { { name = "r1", type = "ubx/ramp" } },
      configurations = {
	 { name = "r1", config = cfg },
      },
   }

   ni = sys:launch({ nodename = nodename, nostart = true, loglevel = LOGLEVEL })
   assert_not_nil(ni)

   local r1 = ni:b("r1")
   local p_out = ubx.port_clone_conn(r1, "out", nil, 1, 7, 0)
   ubx.block_tostate(r1, 'active')
   return r1, p_out
end

--- basic double ramp: first output is start, then increments by slope
function TestRamp:TestBasicDouble()
   local r1, p_out = make({ type_name = "double", start = 1.5, slope = 0.5 },
			  "TestRampDouble")

   local expected = { 1.5, 2.0, 2.5, 3.0 }
   for i = 1, #expected do
      r1:do_step()
      local len, val = p_out:read()
      assert_true(tonumber(len) > 0, "expected output on every step")
      assert_equals(val:tolua(), expected[i])
   end
end

--- native accumulation: a uint64 ramp counts exactly beyond 2^53
--- (a double-internal implementation would get stuck at 2^53)
function TestRamp:TestUint64Exact()
   local r1, p_out = make({ type_name = "uint64_t", start = 2^53, slope = 1 },
			  "TestRampU64")

   local base = 9007199254740992ULL	-- 2^53
   for i = 0, 4 do
      r1:do_step()
      local _, val = p_out:read()
      local got = val:cdata()[0]
      assert_true(got == base + i,
		  ("step %d: got %s, expected %s"):format(i, tostring(got),
							  tostring(base + i)))
   end
end

--- start/slope have the native type: a 64-bit start beyond 2^53 given
--- as a ULL literal in the model is exact
function TestRamp:TestUint64ExactConfig()
   local start = 1700000000000000001ULL
   local r1, p_out = make({ type_name = "uint64_t", start = start, slope = 2ULL },
			  "TestRampU64Cfg")

   for i = 0, 2 do
      r1:do_step()
      local _, val = p_out:read()
      local got = val:cdata()[0]
      assert_true(got == start + 2 * i,
		  ("step %d: got %s, expected %s"):format(i, tostring(got),
							  tostring(start + 2 * i)))
   end
end

--- start/slope are added by preinit, so block_create applies them,
--- and type can't change after preinit
function TestRamp:TestPreinitConfigs()
   ni = ubx.node_create("TestRampPreinit", { loglevel = LOGLEVEL })
   ubx.load_module(ni, "stdtypes")
   ubx.load_module(ni, "ramp")

   local r1 = ubx.block_create(ni, "ubx/ramp", "r1",
			       { type_name = "int64_t", start = -5, slope = 3 })
   assert_equals(ffi.string(r1:c("start").type.name), "int64_t")
   assert_equals(r1:c("slope"):tolua(), 3)
   lu.assert_error_msg_contains("preinit config", ubx.set_config, r1, "type_name", "double")
end

--- unsigned wrap-around is well-defined
function TestRamp:TestUint8Wrap()
   local r1, p_out = make({ type_name = "uint8_t", start = 254, slope = 1 },
			  "TestRampU8")

   local expected = { 254, 255, 0, 1 }
   for i = 1, #expected do
      r1:do_step()
      local _, val = p_out:read()
      assert_equals(tonumber(val:cdata()[0]), expected[i])
   end
end

--- data_len > 1 with per-element start and scalar (broadcast) slope
function TestRamp:TestPerElement()
   local r1, p_out = make({ type_name = "int32_t", data_len = 3,
			    start = { 0, 10, -10 }, slope = 2 },
			  "TestRampVec")

   local expected = { { 0, 10, -10 }, { 2, 12, -8 }, { 4, 14, -6 } }
   for i = 1, #expected do
      r1:do_step()
      local len, val = p_out:read()
      assert_equals(tonumber(len), 3)
      assert_equals(val:tolua(), expected[i])
   end
end

--- restart resets the ramp to start
function TestRamp:TestRestartResets()
   local r1, p_out = make({ type_name = "double", start = 5, slope = 1 },
			  "TestRampRestart")

   local function step_and_read()
      r1:do_step()
      local _, val = p_out:read()
      return val:tolua()
   end

   assert_equals(step_and_read(), 5)
   assert_equals(step_and_read(), 6)

   ubx.block_tostate(r1, 'inactive')
   ubx.block_tostate(r1, 'active')

   assert_equals(step_and_read(), 5)
end

--- invalid configs are refused (values are converted by LuaJIT
--- without range checks)
function TestRamp:TestBadConfig()
   -- slope missing
   assert_true(not pcall(make, { type_name = "double" }, "TestRampNoSlope"))
   -- unsupported type
   assert_true(not pcall(make, { type_name = "char", slope = 1 }, "TestRampBadType"))
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
