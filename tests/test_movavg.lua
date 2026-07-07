--
-- Tests for the fixed-window moving-average block (ubx/movavg)
--
-- Feeds known sequences and checks the moving average, the warm-up
-- (partial window) behaviour, the runtime-typed ports and integer
-- rounding, and that NODATA produces no output.
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

TestMovavg = {}

function TestMovavg:teardown()
   if ni then ubx.node_rm(ni) end
   ni = nil
end

local function make(cfg, nodename)
   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "movavg" },
      blocks = { { name = "ma1", type = "ubx/movavg" } },
      configurations = {
	 { name = "ma1", config = cfg },
      },
   }

   ni = sys:launch({ nodename = nodename, nostart = true, loglevel = LOGLEVEL })
   assert_not_nil(ni)

   local ma1 = ni:b("ma1")
   local p_in = ubx.port_clone_conn(ma1, "in", 1, nil, 7, 0)
   local p_out = ubx.port_clone_conn(ma1, "out", nil, 1, 7, 0)
   ubx.block_tostate(ma1, 'active')
   return ma1, p_in, p_out
end

--
-- Window of 3 over a double signal. Warm-up: the first two outputs
-- average the partial window, then a full sliding window of 3.
--   in:  1    2      3      4      5
--   out: 1  1.5      2      3      4
--
function TestMovavg:TestSlidingDouble()
   local _, p_in, p_out = make({ type = "double", window = 3 }, "TestMovavgDouble")

   local expected = { 1.0, 1.5, 2.0, 3.0, 4.0 }
   local data = { 1, 2, 3, 4, 5 }
   for i, v in ipairs(data) do
      p_in:write(v)
      ni:b("ma1"):do_step()
      local len, val = p_out:read()
      assert_true(tonumber(len) > 0, "expected output on every step")
      assert_true(math.abs(val:tolua() - expected[i]) < 1e-9,
		  "unexpected moving average at step " .. i)
   end
end

--
-- The runtime-typed ports must adopt the configured type, and integer
-- output is rounded to nearest.
--   window = 2, in: 1, 2, 3, 4  =>  avg: 1, 1.5, 2.5, 3.5
--   rounded (round-half-away-from-zero via llround): 1, 2, 3, 4
--
function TestMovavg:TestIntRounding()
   local ma1, p_in, p_out = make({ type = "int32_t", window = 2 }, "TestMovavgInt")

   assert_equals(ffi.string(ubx.port_get(ma1, "in").in_type.name), "int32_t")
   assert_equals(ffi.string(ubx.port_get(ma1, "out").out_type.name), "int32_t")

   local expected = { 1, 2, 3, 4 }
   for i, v in ipairs({ 1, 2, 3, 4 }) do
      p_in:write(v)
      ma1:do_step()
      local _, val = p_out:read()
      assert_equals(val:tolua(), expected[i])
   end
end

--
-- data_len > 1: an independent moving average per vector element.
--   window = 2, data_len = 2
--   in:  {1,10}   {3,20}    {5,30}
--   out: {1,10}   {2,15}    {4,25}
--
function TestMovavg:TestPerElement()
   local ma1, p_in, p_out = make({ type = "double", window = 2, data_len = 2 },
				 "TestMovavgVec")

   assert_equals(tonumber(ubx.port_get(ma1, "in").in_data_len), 2)

   local in_data  = { { 1, 10 }, { 3, 20 }, { 5, 30 } }
   local expected = { { 1, 10 }, { 2, 15 }, { 4, 25 } }
   for i = 1, #in_data do
      p_in:write(in_data[i])
      ma1:do_step()
      local len, val = p_out:read()
      assert_equals(tonumber(len), 2)
      assert_equals(val:tolua(), expected[i])
   end
end

--
-- A window of 1 is a pass-through filter.
--
function TestMovavg:TestWindowOne()
   local _, p_in, p_out = make({ type = "double", window = 1 }, "TestMovavgOne")

   for _, v in ipairs({ 7, -3, 42 }) do
      p_in:write(v)
      ni:b("ma1"):do_step()
      local _, val = p_out:read()
      assert_equals(val:tolua(), v)
   end
end

--
-- No input on the port must produce no output (NODATA).
--
function TestMovavg:TestNoData()
   local ma1, _, p_out = make({ type = "double", window = 3 }, "TestMovavgNoData")

   ma1:do_step()
   local len = p_out:read()
   assert_equals(tonumber(len), 0)
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
