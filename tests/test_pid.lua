--
-- Test the PID controller block
--

local lu = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")
local ffi = require("ffi")

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_WARN

local ni

TestPID = {}

function TestPID:teardown()
   if ni then ubx.node_rm(ni) end
   ni = nil
end

--- Test P-only controller (scalar)
function TestPID:TestPOnly()
   local sys = bd.system {
      imports = { "stdtypes", "lfds_cyclic", "pid" },
      blocks = {
	 { name = "pid1", type = "ubx/pid" },
      },
      configurations = {
	 { name = "pid1", config = { Kp = 2.0 } },
      },
   }

   ni = sys:launch({ nodename = "TestPOnly", nostart = true, loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)

   local pid1 = ni:b("pid1")
   local p_msr = ubx.port_clone_conn(pid1, "msr", 1, nil, 7, 0)
   local p_des = ubx.port_clone_conn(pid1, "des", 1, nil, 7, 0)
   local p_out = ubx.port_clone_conn(pid1, "out", nil, 1, 7, 0)

   ubx.block_tostate(pid1, 'active')

   -- des=10, msr=3 => err=7, out=Kp*err=14
   p_des:write(10.0)
   p_msr:write(3.0)
   pid1:do_step()

   local len, val = p_out:read()
   lu.assert_equals(tonumber(len), 1)
   lu.assert_equals(val:tolua(), 14.0)
end

--- Test PI controller (check integral accumulation)
function TestPID:TestPI()
   local sys = bd.system {
      imports = { "stdtypes", "lfds_cyclic", "pid" },
      blocks = {
	 { name = "pid1", type = "ubx/pid" },
      },
      configurations = {
	 { name = "pid1", config = { Kp = 1.0, Ki = 0.5 } },
      },
   }

   ni = sys:launch({ nodename = "TestPI", nostart = true, loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)

   local pid1 = ni:b("pid1")
   local p_msr = ubx.port_clone_conn(pid1, "msr", 1, nil, 7, 0)
   local p_des = ubx.port_clone_conn(pid1, "des", 1, nil, 7, 0)
   local p_out = ubx.port_clone_conn(pid1, "out", nil, 1, 7, 0)

   ubx.block_tostate(pid1, 'active')

   -- step 1: err=10, integ=10, out = 1*10 + 0.5*10 = 15
   p_des:write(10.0)
   p_msr:write(0.0)
   pid1:do_step()
   local _, val1 = p_out:read()
   lu.assert_equals(val1:tolua(), 15.0)

   -- step 2: err=10, integ=20, out = 1*10 + 0.5*20 = 20
   p_des:write(10.0)
   p_msr:write(0.0)
   pid1:do_step()
   local _, val2 = p_out:read()
   lu.assert_equals(val2:tolua(), 20.0)
end

--- Test PD controller (check derivative)
function TestPID:TestPD()
   local sys = bd.system {
      imports = { "stdtypes", "lfds_cyclic", "pid" },
      blocks = {
	 { name = "pid1", type = "ubx/pid" },
      },
      configurations = {
	 { name = "pid1", config = { Kp = 1.0, Kd = 2.0 } },
      },
   }

   ni = sys:launch({ nodename = "TestPD", nostart = true, loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)

   local pid1 = ni:b("pid1")
   local p_msr = ubx.port_clone_conn(pid1, "msr", 1, nil, 7, 0)
   local p_des = ubx.port_clone_conn(pid1, "des", 1, nil, 7, 0)
   local p_out = ubx.port_clone_conn(pid1, "out", nil, 1, 7, 0)

   ubx.block_tostate(pid1, 'active')

   -- step 1: err=5, no prev => no deriv, out = 1*5 = 5
   p_des:write(10.0)
   p_msr:write(5.0)
   pid1:do_step()
   local _, val1 = p_out:read()
   lu.assert_equals(val1:tolua(), 5.0)

   -- step 2: err=3, deriv=3-5=-2, out = 1*3 + 2*(-2) = -1
   p_des:write(10.0)
   p_msr:write(7.0)
   pid1:do_step()
   local _, val2 = p_out:read()
   lu.assert_equals(val2:tolua(), -1.0)
end

--- Test PID with data_len > 1
function TestPID:TestVectorPID()
   local data_len = 3

   local sys = bd.system {
      imports = { "stdtypes", "lfds_cyclic", "pid" },
      blocks = {
	 { name = "pid1", type = "ubx/pid" },
      },
      configurations = {
	 { name = "pid1", config = {
	      data_len = data_len,
	      Kp = { 1.0, 2.0, 3.0 },
	 } },
      },
   }

   ni = sys:launch({ nodename = "TestVecPID", nostart = true, loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)

   local pid1 = ni:b("pid1")
   local p_msr = ubx.port_clone_conn(pid1, "msr", 1, nil, 7, 0)
   local p_des = ubx.port_clone_conn(pid1, "des", 1, nil, 7, 0)
   local p_out = ubx.port_clone_conn(pid1, "out", nil, 1, 7, 0)

   ubx.block_tostate(pid1, 'active')

   -- err = {10-1, 20-2, 30-3} = {9, 18, 27}
   -- out = {1*9, 2*18, 3*27} = {9, 36, 81}
   p_des:write({ 10.0, 20.0, 30.0 })
   p_msr:write({ 1.0, 2.0, 3.0 })
   pid1:do_step()

   local len, val = p_out:read()
   lu.assert_equals(tonumber(len), data_len)
   lu.assert_equals(val:tolua(), { 9.0, 36.0, 81.0 })
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
