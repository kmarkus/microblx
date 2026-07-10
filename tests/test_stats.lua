--
-- Tests for the generic statistics block (ubx/stats)
--
-- Feeds a known sequence and checks cnt/min/max/mean and the
-- population standard deviation. Also checks the runtime-typed input
-- port (here: double) and that NODATA produces no output.
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

TestStats = {}

function TestStats:teardown()
   if ni then ubx.node_rm(ni) end
   ni = nil
end

--
-- Classic textbook sample: values with mean 5 and population stddev 2.
--   data = { 2, 4, 4, 4, 5, 5, 7, 9 }
--   cnt = 8, min = 2, max = 9, mean = 5
--   population variance = 32/8 = 4  =>  std = 2
--
function TestStats:TestBasicDouble()
   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "stats" },
      blocks = { { name = "st1", type = "ubx/stats" } },
      configurations = {
	 { name = "st1", config = { type = "double" } },
      },
   }

   ni = sys:launch({ nodename = "TestStats", nostart = true, loglevel = LOGLEVEL })
   assert_not_nil(ni)

   local st1 = ni:b("st1")
   local p_in = ubx.port_clone_conn(st1, "in", 1, nil, 7, 0)
   local p_stats = ubx.port_clone_conn(st1, "stats", nil, 1, 7, 0)

   ubx.block_tostate(st1, 'active')

   local data = { 2, 4, 4, 4, 5, 5, 7, 9 }
   local s
   for _, v in ipairs(data) do
      p_in:write(v)
      st1:do_step()
      local len, val = p_stats:read()
      assert_true(tonumber(len) > 0, "expected stats output on every step")
      s = val:cdata()
   end

   assert_equals(tonumber(s.cnt), 8)
   assert_equals(s.min, 2.0)
   assert_equals(s.max, 9.0)
   assert_equals(s.mean, 5.0)
   assert_true(math.abs(s.std - 2.0) < 1e-9, "population stddev should be 2")
end

--
-- The runtime-typed 'in' port must adopt the configured type.
--
function TestStats:TestIntType()
   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "stats" },
      blocks = { { name = "st1", type = "ubx/stats" } },
      configurations = {
	 { name = "st1", config = { type = "int32_t" } },
      },
   }

   ni = sys:launch({ nodename = "TestStatsInt", nostart = true, loglevel = LOGLEVEL })
   assert_not_nil(ni)

   local st1 = ni:b("st1")
   local p_in = ubx.port_clone_conn(st1, "in", 1, nil, 7, 0)
   local p_stats = ubx.port_clone_conn(st1, "stats", nil, 1, 7, 0)

   -- the 'in' port must have been created with the configured type
   assert_equals(ffi.string(ubx.port_get(st1, "in").in_type.name), "int32_t")

   ubx.block_tostate(st1, 'active')

   local s
   for _, v in ipairs({ -3, 0, 3, 6 }) do
      p_in:write(v)
      st1:do_step()
      local _, val = p_stats:read()
      s = val:cdata()
   end

   assert_equals(tonumber(s.cnt), 4)
   assert_equals(s.min, -3.0)
   assert_equals(s.max, 6.0)
   assert_equals(s.mean, 1.5)
end

--
-- data_len > 1: independent statistics per vector element (channel).
--   channel 0 = { 2,4,4,4,5,5,7,9 }  =>  min2 max9 mean5 std2
--   channel 1 = { 1,1,1,1,1,1,1,1 }  =>  min1 max1 mean1 std0
--
function TestStats:TestPerElement()
   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "stats" },
      blocks = { { name = "st1", type = "ubx/stats" } },
      configurations = {
	 { name = "st1", config = { type = "double", data_len = 2 } },
      },
   }

   ni = sys:launch({ nodename = "TestStatsVec", nostart = true, loglevel = LOGLEVEL })
   assert_not_nil(ni)

   local st1 = ni:b("st1")
   local p_in = ubx.port_clone_conn(st1, "in", 1, nil, 7, 0)
   local p_stats = ubx.port_clone_conn(st1, "stats", nil, 1, 7, 0)

   assert_equals(tonumber(ubx.port_get(st1, "in").in_data_len), 2)

   ubx.block_tostate(st1, 'active')

   local ch0 = { 2, 4, 4, 4, 5, 5, 7, 9 }
   local s
   for i = 1, #ch0 do
      p_in:write({ ch0[i], 1 })
      st1:do_step()
      local _, val = p_stats:read()
      s = val:cdata()
   end

   assert_equals(tonumber(s[0].cnt), 8)
   assert_equals(s[0].min, 2.0)
   assert_equals(s[0].max, 9.0)
   assert_equals(s[0].mean, 5.0)
   assert_true(math.abs(s[0].std - 2.0) < 1e-9, "channel 0 stddev should be 2")

   assert_equals(tonumber(s[1].cnt), 8)
   assert_equals(s[1].min, 1.0)
   assert_equals(s[1].max, 1.0)
   assert_equals(s[1].mean, 1.0)
   assert_equals(s[1].std, 0.0)
end

--
-- A single sample: population stddev is well-defined and 0.
--
function TestStats:TestSingleSample()
   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "stats" },
      blocks = { { name = "st1", type = "ubx/stats" } },
      configurations = {
	 { name = "st1", config = { type = "double" } },
      },
   }

   ni = sys:launch({ nodename = "TestStatsSingle", nostart = true, loglevel = LOGLEVEL })
   assert_not_nil(ni)

   local st1 = ni:b("st1")
   local p_in = ubx.port_clone_conn(st1, "in", 1, nil, 7, 0)
   local p_stats = ubx.port_clone_conn(st1, "stats", nil, 1, 7, 0)

   ubx.block_tostate(st1, 'active')

   p_in:write(42.0)
   st1:do_step()
   local _, val = p_stats:read()
   local s = val:cdata()

   assert_equals(tonumber(s.cnt), 1)
   assert_equals(s.min, 42.0)
   assert_equals(s.max, 42.0)
   assert_equals(s.mean, 42.0)
   assert_equals(s.std, 0.0)
end

--
-- No input on the port must produce no output (NODATA).
--
function TestStats:TestNoData()
   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "stats" },
      blocks = { { name = "st1", type = "ubx/stats" } },
      configurations = {
	 { name = "st1", config = { type = "double" } },
      },
   }

   ni = sys:launch({ nodename = "TestStatsNoData", nostart = true, loglevel = LOGLEVEL })
   assert_not_nil(ni)

   local st1 = ni:b("st1")
   ubx.port_clone_conn(st1, "in", 1, nil, 7, 0)
   local p_stats = ubx.port_clone_conn(st1, "stats", nil, 1, 7, 0)

   ubx.block_tostate(st1, 'active')

   st1:do_step()
   local len = p_stats:read()
   assert_equals(tonumber(len), 0)
end

--
-- stats_output_rate throttles port output: with a large interval only
-- the first step emits, later (immediate) steps are suppressed while
-- the statistics keep updating.
--
function TestStats:TestOutputRateThrottle()
   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "stats" },
      blocks = { { name = "st1", type = "ubx/stats" } },
      configurations = {
	 -- deliberately larger than any possible system uptime: the first
	 -- emission must happen regardless of the monotonic clock value
	 { name = "st1", config = { type = "double", stats_output_rate = 1e9 } },
      },
   }

   ni = sys:launch({ nodename = "TestStatsThrottle", nostart = true, loglevel = LOGLEVEL })
   assert_not_nil(ni)

   local st1 = ni:b("st1")
   local p_in = ubx.port_clone_conn(st1, "in", 1, nil, 7, 0)
   local p_stats = ubx.port_clone_conn(st1, "stats", nil, 1, 7, 0)

   ubx.block_tostate(st1, 'active')

   -- first step emits
   p_in:write(1.0)
   st1:do_step()
   local len = p_stats:read()
   assert_true(tonumber(len) > 0, "first step should emit within throttle window")

   -- subsequent immediate steps are throttled (no output)
   for _, v in ipairs({ 2.0, 3.0, 4.0 }) do
      p_in:write(v)
      st1:do_step()
      len = p_stats:read()
      assert_equals(tonumber(len), 0)
   end
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
