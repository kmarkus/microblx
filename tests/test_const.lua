--
-- Tests for the const blocks (ubx/cconst, ubx/iconst).
--
-- Verifies that the configured initial 'value' is emitted and that
-- writing to the new 'in' port updates the held value.
--

local lu = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")
local ffi = require("ffi")

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_INFO

local assert_equals = lu.assert_equals
local assert_not_nil = lu.assert_not_nil
local assert_true = lu.assert_true

local ni

TestConst = {}

function TestConst:teardown()
   if ni then ubx.node_rm(ni) end
   ni = nil
end

--
-- cconst: scalar
--
function TestConst:TestCConstScalar()
   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "cconst" },
      blocks = { { name = "cc1", type = "ubx/cconst" } },
      configurations = {
	 { name = "cc1", config = { type_name = "int", value = 42 } }
      }
   }
   ni = sys:launch{ nodename = "TestCConstScalar", loglevel=LOGLEVEL }
   assert_not_nil(ni)

   local cc1 = ni:b("cc1")
   local pin = ubx.port_clone_conn(cc1, "in", 1, nil, 7, 0)
   local pout = ubx.port_clone_conn(cc1, "out", nil, 1, 7, 0)

   -- initial step emits the configured value
   cc1:do_step()
   local len, val = pout:read()
   assert_equals(tonumber(len), 1)
   assert_equals(val:tolua(), 42)

   -- write a new value via 'in' and step: 'out' must reflect the update
   pin:write(123)
   cc1:do_step()
   len, val = pout:read()
   assert_equals(tonumber(len), 1)
   assert_equals(val:tolua(), 123)

   -- with no new write on 'in' the previous value sticks
   cc1:do_step()
   len, val = pout:read()
   assert_equals(tonumber(len), 1)
   assert_equals(val:tolua(), 123)

   -- another update
   pin:write(-7)
   cc1:do_step()
   len, val = pout:read()
   assert_equals(tonumber(len), 1)
   assert_equals(val:tolua(), -7)
end

--
-- cconst: array (data_len > 1)
--
function TestConst:TestCConstArray()
   local DATA_LEN = 4
   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "cconst" },
      blocks = { { name = "cc1", type = "ubx/cconst" } },
      configurations = {
	 { name = "cc1", config = {
	      type_name = "int", data_len = DATA_LEN,
	      value = { 10, 20, 30, 40 } } }
      }
   }
   ni = sys:launch{ nodename = "TestCConstArray", loglevel=LOGLEVEL }
   assert_not_nil(ni)

   local cc1 = ni:b("cc1")
   local pin = ubx.port_clone_conn(cc1, "in", 1, nil, 7, 0)
   local pout = ubx.port_clone_conn(cc1, "out", nil, 1, 7, 0)

   cc1:do_step()
   local len, val = pout:read()
   assert_equals(tonumber(len), DATA_LEN)
   assert_equals(val:tolua(), { 10, 20, 30, 40 })

   pin:write({ 1, 2, 3, 4 })
   cc1:do_step()
   len, val = pout:read()
   assert_equals(tonumber(len), DATA_LEN)
   assert_equals(val:tolua(), { 1, 2, 3, 4 })
end

--
-- iconst: scalar. The new 'in' port must update the held value
-- before it is returned by read().
--
function TestConst:TestIConstScalar()
   local nd = ubx.node_create("TestIConstScalar", { loglevel = LOGLEVEL })
   ubx.load_module(nd, "stdtypes")
   ubx.load_module(nd, "lfrb")
   ubx.load_module(nd, "iconst")
   ubx.ffi_load_types(nd)

   local ic = ubx.block_create(nd, "ubx/iconst", "ic1")
   assert_not_nil(ic)
   -- 'value' is added by init(), so configure with do_configure
   ubx.do_configure(ic, { type_name = "int", value = 77 })
   assert_equals(ubx.block_tostate(ic, 'active'), 0)

   local pin = ubx.port_clone_conn(ic, "in", 1, nil, 7, 0)

   -- a ubx_data_t to read the constant into
   local rdat = ubx.data_alloc(nd, "int", 1)

   -- initial read: configured value
   local n = ubx.interaction_read(ic, rdat)
   assert_equals(tonumber(n), 1)
   assert_equals(rdat:tolua(), 77)

   -- update via 'in' then read again
   pin:write(555)
   n = ubx.interaction_read(ic, rdat)
   assert_equals(tonumber(n), 1)
   assert_equals(rdat:tolua(), 555)

   -- no further update: previous value sticks
   n = ubx.interaction_read(ic, rdat)
   assert_equals(tonumber(n), 1)
   assert_equals(rdat:tolua(), 555)

   ubx.node_rm(nd)
end

--
-- iconst: array
--
function TestConst:TestIConstArray()
   local DATA_LEN = 3
   local nd = ubx.node_create("TestIConstArray", { loglevel = LOGLEVEL })
   ubx.load_module(nd, "stdtypes")
   ubx.load_module(nd, "lfrb")
   ubx.load_module(nd, "iconst")
   ubx.ffi_load_types(nd)

   local ic = ubx.block_create(nd, "ubx/iconst", "ic1")
   assert_not_nil(ic)
   ubx.do_configure(ic, { type_name = "int",
			  data_len = DATA_LEN,
			  value = { 9, 8, 7 } })
   assert_equals(ubx.block_tostate(ic, 'active'), 0)

   local pin = ubx.port_clone_conn(ic, "in", 1, nil, 7, 0)
   local rdat = ubx.data_alloc(nd, "int", DATA_LEN)

   local n = ubx.interaction_read(ic, rdat)
   assert_equals(tonumber(n), DATA_LEN)
   assert_equals(rdat:tolua(), { 9, 8, 7 })

   pin:write({ 100, 200, 300 })
   n = ubx.interaction_read(ic, rdat)
   assert_equals(tonumber(n), DATA_LEN)
   assert_equals(rdat:tolua(), { 100, 200, 300 })

   ubx.node_rm(nd)
end

if not _RUNNER then os.exit( lu.LuaUnit.run() ) end
