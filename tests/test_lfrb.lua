--
-- Test lfrb (lock-free ring buffer) as alternative iblock
--

local lu = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")
local u = require("utils")
local ffi = require("ffi")

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_WARN

local ni

TestLfrb = {}

function TestLfrb:setup()
   ubx.reset_block_uid()
end

function TestLfrb:teardown()
   if ni then ubx.node_rm(ni) end
   ni = nil
end

--- Test basic connection using lfrb iblock
function TestLfrb:TestSimpleConnection()
   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "saturation" },
      blocks = {
	 { name = "sat1", type = "ubx/saturation" },
	 { name = "sat2", type = "ubx/saturation" },
      },
      configurations = {
	 { name = "sat1", config = {
	      type="double",
	      lower_limits = -10, upper_limits = 10 } },
	 { name = "sat2", config = {
	      type="double",
	      lower_limits = -5, upper_limits = 5 } },
      },
      connections = {
	 { src = "sat1.out", tgt = "sat2.in", config = { buffer_len = 4 } },
      },
   }

   local numerr = sys:validate(false)
   lu.assert_equals(numerr, 0)
   ni = sys:launch({ nodename = "TestLfrb", loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)

   local sat1 = ni:b("sat1")
   local sat2 = ni:b("sat2")
   local pin1 = ubx.port_clone_conn(sat1, "in", 1, nil, 7, 0)
   local pout2 = ubx.port_clone_conn(sat2, "out", nil, 1, 7, 0)

   -- value within both limits
   pin1:write(3.0)
   sat1:do_step()
   sat2:do_step()
   local len, val = pout2:read()
   lu.assert_equals(tonumber(len), 1)
   lu.assert_equals(val:tolua(), 3.0)

   -- value clipped by sat1 to 10, then clipped by sat2 to 5
   pin1:write(100.0)
   sat1:do_step()
   sat2:do_step()
   len, val = pout2:read()
   lu.assert_equals(tonumber(len), 1)
   lu.assert_equals(val:tolua(), 5.0)
end

--- Test lfrb with vector data
function TestLfrb:TestVectorData()
   local DATA_LEN = 3

   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "saturation" },
      blocks = {
	 { name = "sat1", type = "ubx/saturation" },
	 { name = "sat2", type = "ubx/saturation" },
      },
      configurations = {
	 { name = "sat1", config = {
	      type="double",
	      data_len = DATA_LEN,
	      lower_limits = { -10, -10, -10 },
	      upper_limits = { 10, 10, 10 } } },
	 { name = "sat2", config = {
	      type="double",
	      data_len = DATA_LEN,
	      lower_limits = { -5, -5, -5 },
	      upper_limits = { 5, 5, 5 } } },
      },
      connections = {
	 { src = "sat1.out", tgt = "sat2.in", config = { buffer_len = 4 } },
      },
   }

   ni = sys:launch({ nodename = "TestLfrbVec", loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)

   local sat1 = ni:b("sat1")
   local sat2 = ni:b("sat2")
   local pin1 = ubx.port_clone_conn(sat1, "in", 1, nil, 7, 0)
   local pout2 = ubx.port_clone_conn(sat2, "out", nil, 1, 7, 0)

   pin1:write({ 3.0, -100.0, 7.0 })
   sat1:do_step()
   sat2:do_step()
   local len, val = pout2:read()
   lu.assert_equals(tonumber(len), DATA_LEN)
   -- sat1: {3, -10, 7}, sat2: {3, -5, 5}
   lu.assert_equals(val:tolua(), { 3.0, -5.0, 5.0 })
end

--- Test lfrb connection table is correct
function TestLfrb:TestConnTab()
   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "saturation" },
      blocks = {
	 { name = "sat1", type = "ubx/saturation" },
	 { name = "sat2", type = "ubx/saturation" },
      },
      configurations = {
	 { name = "sat1", config = {
	      type="double",
	      lower_limits = -10, upper_limits = 10 } },
	 { name = "sat2", config = {
	      type="double",
	      lower_limits = -5, upper_limits = 5 } },
      },
      connections = {
	 { src = "sat1.out", tgt = "sat2.in", config = { buffer_len = 4 } },
      },
   }

   ni = sys:launch({ nodename = "TestLfrbConn", loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)

   local conntab = ubx.build_conntab(ni)
   local conntab_exp = {
      sat1 = {
	 { ['in'] = { incoming = {}, outgoing = {} } },
	 { out = { incoming = {}, outgoing = { "i_00000001" } } }
      },
      sat2 = {
	 { ['in'] = { incoming = { "i_00000001" }, outgoing = {} } },
	 { out = { incoming = {}, outgoing = {} } }
      }
   }
   lu.assert_equals(conntab, conntab_exp)
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
