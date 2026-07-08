--
-- port_write with LuaJIT FFI cdata input.
--
-- Mirrors the data_set cdata tests in test_data_init.lua, but at the
-- port_write layer: a numeric cdata flows through port_write ->
-- port_alloc_write_sample -> sample:set (data_set), gets converted to
-- the port's underlying ctype, and round-trips through an lfrb
-- connection just like a plain Lua number would.
--

local lu = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")
local ffi = require("ffi")

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_WARN

local ni

TestPortWriteCdata = {}

function TestPortWriteCdata:setup()
   ubx.reset_block_uid()
end

function TestPortWriteCdata:teardown()
   if ni then ubx.node_rm(ni) end
   ni = nil
end

local function make_double_pipe(nodename)
   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "saturation" },
      blocks = {
	 { name = "sat1", type = "ubx/saturation" },
	 { name = "sat2", type = "ubx/saturation" },
      },
      configurations = {
	 { name = "sat1", config = { type="double", lower_limits = -1e9, upper_limits = 1e9 } },
	 { name = "sat2", config = { type="double", lower_limits = -1e9, upper_limits = 1e9 } },
      },
      connections = {
	 { src = "sat1.out", tgt = "sat2.in", config = { buffer_len = 4 } },
      },
   }
   ni = sys:launch({ nodename = nodename, loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)
   local sat1 = ni:b("sat1")
   local sat2 = ni:b("sat2")
   local pin1 = ubx.port_clone_conn(sat1, "in", 1, nil, 7, 0)
   local pout2 = ubx.port_clone_conn(sat2, "out", nil, 1, 7, 0)
   return sat1, sat2, pin1, pout2
end

local function step_and_read(sat1, sat2, pout)
   sat1:do_step()
   sat2:do_step()
   local len, val = pout:read()
   lu.assert_equals(tonumber(len), 1)
   return val:tolua()
end

function TestPortWriteCdata:TestUint64CdataInput()
   local sat1, sat2, pin, pout = make_double_pipe("PortWriteU64")
   pin:write(ffi.new("uint64_t", 42))
   lu.assert_equals(step_and_read(sat1, sat2, pout), 42.0)
end

function TestPortWriteCdata:TestInt64CdataInput()
   local sat1, sat2, pin, pout = make_double_pipe("PortWriteI64")
   pin:write(ffi.new("int64_t", -42))
   lu.assert_equals(step_and_read(sat1, sat2, pout), -42.0)
end

function TestPortWriteCdata:TestInt32CdataInput()
   local sat1, sat2, pin, pout = make_double_pipe("PortWriteI32")
   pin:write(ffi.new("int32_t", 17))
   lu.assert_equals(step_and_read(sat1, sat2, pout), 17.0)
end

function TestPortWriteCdata:TestUint8CdataInput()
   local sat1, sat2, pin, pout = make_double_pipe("PortWriteU8")
   pin:write(ffi.new("uint8_t", 255))
   lu.assert_equals(step_and_read(sat1, sat2, pout), 255.0)
end

-- LL/ULL literals exercise the same path as ffi.new, but make sure
-- the path that json.decode_bigint produces (LL/ULL via loadstring)
-- works end-to-end.
function TestPortWriteCdata:TestULLLiteralInput()
   local sat1, sat2, pin, pout = make_double_pipe("PortWriteULL")
   pin:write(123ULL)
   lu.assert_equals(step_and_read(sat1, sat2, pout), 123.0)
end

function TestPortWriteCdata:TestLLLiteralInput()
   local sat1, sat2, pin, pout = make_double_pipe("PortWriteLL")
   pin:write(-99LL)
   lu.assert_equals(step_and_read(sat1, sat2, pout), -99.0)
end

function TestPortWriteCdata:TestDoubleCdataRejected()
   local _, _, pin = make_double_pipe("PortWriteRejDbl")
   local ok, err = pcall(function() pin:write(ffi.new("double", 1.5)) end)
   lu.assert_false(ok)
   -- The new refined message; either port_write's own rejection or the
   -- data_set fallthrough message would be acceptable -- check for the
   -- distinctive substring.
   lu.assert_str_contains(err, "invalid cdata")
end

function TestPortWriteCdata:TestPointerCdataRejected()
   local _, _, pin = make_double_pipe("PortWriteRejPtr")
   local ok, err = pcall(function() pin:write(ffi.new("void *", nil)) end)
   lu.assert_false(ok)
   lu.assert_str_contains(err, "invalid cdata")
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
