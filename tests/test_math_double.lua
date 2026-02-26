--
-- Test the math_double function block
--

local lu = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")
local ffi = require("ffi")

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_WARN
ubx.color = false

local ni

TestMathDouble = {}

function TestMathDouble:teardown()
   if ni then ubx.node_rm(ni) end
   ni = nil
end

local function launch_math(func, mul, add, data_len)
   data_len = data_len or 1
   local cfg = { func = func }
   if data_len > 1 then cfg.data_len = data_len end
   if mul then cfg.mul = mul end
   if add then cfg.add = add end

   local sys = bd.system {
      imports = { "stdtypes", "lfds_cyclic", "math_double" },
      blocks = {
	 { name = "m1", type = "ubx/math_double" },
      },
      configurations = {
	 { name = "m1", config = cfg },
      },
   }

   ni = sys:launch({ nodename = "TestMath", nostart = true, loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)

   local m1 = ni:b("m1")
   local px = ubx.port_clone_conn(m1, "x", 1, nil, 7, 0)
   local py = ubx.port_clone_conn(m1, "y", nil, 1, 7, 0)
   ubx.block_tostate(m1, 'active')
   return m1, px, py
end

local eps = 1e-9

--- Test sin function
function TestMathDouble:TestSin()
   local m1, px, py = launch_math("sin")

   px:write(0.0)
   m1:do_step()
   local _, val = py:read()
   lu.assert_almost_equals(val:tolua(), 0.0, eps)

   px:write(math.pi / 2)
   m1:do_step()
   _, val = py:read()
   lu.assert_almost_equals(val:tolua(), 1.0, eps)
end

--- Test cos function
function TestMathDouble:TestCos()
   local m1, px, py = launch_math("cos")

   px:write(0.0)
   m1:do_step()
   local _, val = py:read()
   lu.assert_almost_equals(val:tolua(), 1.0, eps)

   px:write(math.pi)
   m1:do_step()
   _, val = py:read()
   lu.assert_almost_equals(val:tolua(), -1.0, eps)
end

--- Test sqrt function
function TestMathDouble:TestSqrt()
   local m1, px, py = launch_math("sqrt")

   px:write(9.0)
   m1:do_step()
   local _, val = py:read()
   lu.assert_almost_equals(val:tolua(), 3.0, eps)

   px:write(2.0)
   m1:do_step()
   _, val = py:read()
   lu.assert_almost_equals(val:tolua(), math.sqrt(2.0), eps)
end

--- Test fabs function
function TestMathDouble:TestFabs()
   local m1, px, py = launch_math("fabs")

   px:write(-42.5)
   m1:do_step()
   local _, val = py:read()
   lu.assert_almost_equals(val:tolua(), 42.5, eps)

   px:write(42.5)
   m1:do_step()
   _, val = py:read()
   lu.assert_almost_equals(val:tolua(), 42.5, eps)
end

--- Test mul and add parameters
function TestMathDouble:TestMulAdd()
   -- y = sin(x) * 2.0 + 1.0
   local m1, px, py = launch_math("sin", 2.0, 1.0)

   px:write(math.pi / 2)
   m1:do_step()
   local _, val = py:read()
   -- sin(pi/2) = 1.0, *2 = 2.0, +1 = 3.0
   lu.assert_almost_equals(val:tolua(), 3.0, eps)
end

--- Test with data_len > 1
function TestMathDouble:TestVectorSqrt()
   local data_len = 3
   local m1, px, py = launch_math("sqrt", nil, nil, data_len)

   px:write({ 4.0, 9.0, 16.0 })
   m1:do_step()
   local len, val = py:read()
   lu.assert_equals(tonumber(len), data_len)
   local res = val:tolua()
   lu.assert_almost_equals(res[1], 2.0, eps)
   lu.assert_almost_equals(res[2], 3.0, eps)
   lu.assert_almost_equals(res[3], 4.0, eps)
end

--- Test exp function
function TestMathDouble:TestExp()
   local m1, px, py = launch_math("exp")

   px:write(0.0)
   m1:do_step()
   local _, val = py:read()
   lu.assert_almost_equals(val:tolua(), 1.0, eps)

   px:write(1.0)
   m1:do_step()
   _, val = py:read()
   lu.assert_almost_equals(val:tolua(), math.exp(1.0), eps)
end

--- Test log function
function TestMathDouble:TestLog()
   local m1, px, py = launch_math("log")

   px:write(1.0)
   m1:do_step()
   local _, val = py:read()
   lu.assert_almost_equals(val:tolua(), 0.0, eps)

   px:write(math.exp(1.0))
   m1:do_step()
   _, val = py:read()
   lu.assert_almost_equals(val:tolua(), 1.0, eps)
end

os.exit(lu.LuaUnit.run())
