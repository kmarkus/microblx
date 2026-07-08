--
-- Tests for the exponentially-weighted moving-average block (ubx/ewma)
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

TestEwma = {}

function TestEwma:teardown()
   if ni then ubx.node_rm(ni) end
   ni = nil
end

local function make(cfg, nodename)
   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "ewma" },
      blocks = { { name = "ew1", type = "ubx/ewma" } },
      configurations = {
	 { name = "ew1", config = cfg },
      },
   }

   ni = sys:launch({ nodename = nodename, nostart = true, loglevel = LOGLEVEL })
   assert_not_nil(ni)

   local ew1 = ni:b("ew1")
   local p_in = ubx.port_clone_conn(ew1, "in", 1, nil, 7, 0)
   local p_out = ubx.port_clone_conn(ew1, "out", nil, 1, 7, 0)
   ubx.block_tostate(ew1, 'active')
   return ew1, p_in, p_out
end

--
-- alpha = 0.5: the first sample seeds y, then y += 0.5 (x - y).
--   in: 1, 3,  3,   11
--   y:  1, 2,  2.5, 6.75
--
function TestEwma:TestBasic()
   local ew1, p_in, p_out = make({ type = "double", alpha = 0.5 }, "TestEwmaBasic")

   local in_data  = { 1, 3, 3, 11 }
   local expected = { 1, 2, 2.5, 6.75 }
   for i, v in ipairs(in_data) do
      p_in:write(v)
      ew1:do_step()
      local len, val = p_out:read()
      assert_true(tonumber(len) > 0, "expected output on every step")
      assert_true(math.abs(val:tolua() - expected[i]) < 1e-9,
		  "unexpected ewma at step " .. i)
   end
end

--
-- alpha = 1 is a pass-through.
--
function TestEwma:TestAlphaOne()
   local ew1, p_in, p_out = make({ type = "double", alpha = 1.0 }, "TestEwmaOne")

   for _, v in ipairs({ 7, -3, 42 }) do
      p_in:write(v)
      ew1:do_step()
      local _, val = p_out:read()
      assert_equals(val:tolua(), v)
   end
end

--
-- data_len > 1: an independent filter per element; integer rounding.
--   alpha = 0.5, in: {0,100} {10,0} => y: {0,100} {5,50}
--
function TestEwma:TestPerElementInt()
   local ew1, p_in, p_out = make({ type = "int32_t", alpha = 0.5, data_len = 2 },
				 "TestEwmaVec")

   assert_equals(ffi.string(ubx.port_get(ew1, "in").in_type.name), "int32_t")
   assert_equals(tonumber(ubx.port_get(ew1, "in").in_data_len), 2)

   local in_data  = { { 0, 100 }, { 10, 0 } }
   local expected = { { 0, 100 }, { 5, 50 } }
   for i = 1, #in_data do
      p_in:write(in_data[i])
      ew1:do_step()
      local len, val = p_out:read()
      assert_equals(tonumber(len), 2)
      assert_equals(val:tolua(), expected[i])
   end
end

--
-- No input on the port must produce no output (NODATA).
--
function TestEwma:TestNoData()
   local ew1, _, p_out = make({ type = "double", alpha = 0.5 }, "TestEwmaNoData")

   ew1:do_step()
   local len = p_out:read()
   assert_equals(tonumber(len), 0)
end

--
-- An out-of-range alpha is refused at init.
--
function TestEwma:TestBadAlpha()
   assert_true(not pcall(make, { type = "double", alpha = 0 }, "TestEwmaBadAlpha1"))
   assert_true(not pcall(make, { type = "double", alpha = 1.5 }, "TestEwmaBadAlpha2"))
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
