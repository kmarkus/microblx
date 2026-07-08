--
-- Tests for the runtime-typed saturation block (ubx/saturation)
--

local lu = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")
local ffi = require("ffi")

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_WARN
local CHECK_VERBOSE = false

local ni

TestSaturation = {}

function TestSaturation:teardown()
   if ni then ubx.node_rm(ni) end
   ni = nil
end

local function make(cfg, nodename)
   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "saturation" },
      blocks = { { name = "sat1", type = "ubx/saturation" } },
      configurations = {
	 { name = "sat1", config = cfg },
      }
   }

   local num_err = sys:validate(CHECK_VERBOSE)
   lu.assert_equals(num_err, 0)
   ni = sys:launch({ nodename = nodename, loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)

   local sat1 = ni:b("sat1")
   local pin = ubx.port_clone_conn(sat1, "in", 1, nil, 7, 0)
   local pout = ubx.port_clone_conn(sat1, "out", nil, 1, 7, 0)
   return sat1, pin, pout
end

function TestSaturation:TestLen1()
   local sat1, pin, pout = make(
      { type = "double", lower_limits = -10, upper_limits = 3.3 }, "TestLen1")

   local in_data =  { 1, 0, 2, 5,  10000, 3.2, -1, -1000, -9.9999, -10.1 }
   local exp_data = { 1, 0, 2, 3.3, 3.3,  3.2, -1, -10,   -9.9999, -10 }
   assert(#in_data == #exp_data)

   for i = 1, #in_data do
      pin:write(in_data[i])
      sat1:do_step()
      local len, val = pout:read()
      lu.assert_equals(tonumber(len), 1)
      lu.assert_equals(val:tolua(), exp_data[i])
   end
end

function TestSaturation:TestLen5()
   local data_len = 5

   local sat1, pin, pout = make({
	 type = "double",
	 data_len = data_len,
	 lower_limits = { -1, -2, -3, -4, -5 },
	 upper_limits = { 1, 22, 333, 4444, 55555 } }, "TestLen5")

   local in_data =  {
      { 0, 0, 0, 0, 0 },
      { 5, 5, 5, 5, 5 },
      { 1, 400, 400, 400, 400 },
      { 22, 333, 4444, 55555, 666666 },
      { 55555, 4444, 333, 22, 1 },

      { -1.1, -2.2, -3.3, -4.4, -5.5 },
      { 0, -10, 0, -10, 0 },
      { -666666, -666666, -666666, -666666, -6666660 },
   }

   local exp_data = {
      { 0, 0, 0, 0, 0 },
      { 1, 5, 5, 5, 5 },
      { 1, 22, 333, 400, 400 },
      { 1, 22, 333, 4444, 55555 },
      { 1, 22, 333, 22, 1 },

      { -1, -2, -3, -4, -5 },
      { 0, -2, 0, -4, 0 },
      { -1, -2, -3, -4, -5 }
   }

   assert(#in_data == #exp_data)
   for i = 1, #in_data do
      assert(#in_data[i] == data_len)
      assert(#exp_data[i] == data_len)
   end

   for i = 1, #in_data do
      pin:write(in_data[i])
      sat1:do_step()
      local len, val = pout:read()
      lu.assert_equals(tonumber(len), data_len)
      lu.assert_equals(val:tolua(), exp_data[i])
   end
end

--- scalar limits broadcast over a vector signal
function TestSaturation:TestScalarBroadcast()
   local sat1, pin, pout = make({
	 type = "double", data_len = 3,
	 lower_limits = -1, upper_limits = 1 }, "TestScalarBroadcast")

   pin:write({ -5, 0.5, 5 })
   sat1:do_step()
   local len, val = pout:read()
   lu.assert_equals(tonumber(len), 3)
   lu.assert_equals(val:tolua(), { -1, 0.5, 1 })
end

--- integer signals: pass-through is exact, clamped values take the limit
function TestSaturation:TestInt32()
   local sat1, pin, pout = make({
	 type = "int32_t", lower_limits = -100, upper_limits = 100 }, "TestInt32")

   local in_data =  { 0, 99, 101, -99, -101, 100, -100 }
   local exp_data = { 0, 99, 100, -99, -100, 100, -100 }

   for i = 1, #in_data do
      pin:write(in_data[i])
      sat1:do_step()
      local _, val = pout:read()
      lu.assert_equals(val:tolua(), exp_data[i])
   end
end

--- lower > upper is refused at init
function TestSaturation:TestInvalidLimits()
   lu.assert_false(pcall(make,
			 { type = "double", lower_limits = 1, upper_limits = -1 },
			 "TestInvalidLimits"))
end

--- limits of length != 1 and != data_len are refused at init
function TestSaturation:TestInvalidLimitLen()
   lu.assert_false(pcall(make,
			 { type = "double", data_len = 3,
			   lower_limits = { 0, 0 }, upper_limits = 1 },
			 "TestInvalidLimitLen"))
end

--- an unsupported type is refused at init
function TestSaturation:TestInvalidType()
   lu.assert_false(pcall(make,
			 { type = "char", lower_limits = 0, upper_limits = 1 },
			 "TestInvalidType"))
end

if not _RUNNER then os.exit( lu.LuaUnit.run() ) end
