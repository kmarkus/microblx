--
-- Test overruns reporting and allow_partial of the lfrb and
-- lfds_cyclic iblocks
--

local lu = require("luaunit")
local ubx = require("ubx")
local ffi = require("ffi")

local assert_equals = lu.assert_equals
local assert_not_nil = lu.assert_not_nil

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_CRIT

local nd

TestIblockOverruns = {}

function TestIblockOverruns:teardown()
   if nd then ubx.node_rm(nd) end
   nd = nil
end

local function setup_node(name, mod)
   nd = ubx.node_create(name, { loglevel = LOGLEVEL })
   ubx.load_module(nd, "stdtypes")
   ubx.load_module(nd, mod)
   ubx.ffi_load_types(nd)
end

local function create_ib(btype, config)
   local ib = ubx.block_create(nd, btype, "ib1")
   assert_not_nil(ib)
   ubx.do_configure(ib, config)
   assert_equals(ubx.block_tostate(ib, 'active'), 0)
   return ib
end

--- writing buffer_len+1 msgs overwrites the oldest and reports an overrun
local function check_overruns(btype)
   local ib = create_ib(btype, {
      type_name = "int", buffer_len = 2, loglevel_overruns = -1 })

   local po = ubx.port_clone_conn(ib, "overruns", nil, 1, 7, 0)

   local wdat = ubx.data_alloc(nd, "int", 1)
   local rdat = ubx.data_alloc(nd, "int", 1)

   for v = 1, 3 do
      ubx.data_set(wdat, v)
      ubx.interaction_write(ib, wdat)
   end

   -- one overrun reported on the overruns port
   local len, val = po:read()
   assert_equals(tonumber(len), 1)
   assert_equals(tonumber(val:tolua()), 1)

   -- oldest msg was overwritten: 2, 3 remain
   assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 1)
   assert_equals(rdat:tolua(), 2)
   assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 1)
   assert_equals(rdat:tolua(), 3)
   assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 0)
end

--- partial msgs are accepted with allow_partial=1 and dropped without
local function check_allow_partial(btype)
   local ib = create_ib(btype, {
      type_name = "int", data_len = 4, buffer_len = 2, allow_partial = 1 })

   local wdat = ubx.data_alloc(nd, "int", 2)
   local rdat = ubx.data_alloc(nd, "int", 2)

   ubx.data_set(wdat, { 7, 8 })
   ubx.interaction_write(ib, wdat)

   assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 2)
   assert_equals(rdat:tolua(), { 7, 8 })
end

local function check_partial_rejected(btype)
   local ib = create_ib(btype, {
      type_name = "int", data_len = 4, buffer_len = 2 })

   local wdat = ubx.data_alloc(nd, "int", 2)
   local rdat = ubx.data_alloc(nd, "int", 2)

   -- len 2 != data_len 4 and allow_partial unset: dropped
   ubx.data_set(wdat, { 7, 8 })
   ubx.interaction_write(ib, wdat)

   assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 0)
end

function TestIblockOverruns:TestLfrbOverruns()
   setup_node("TestLfrbOverruns", "lfrb")
   check_overruns("ubx/lfrb")
end

function TestIblockOverruns:TestLfrbAllowPartial()
   setup_node("TestLfrbPartial", "lfrb")
   check_allow_partial("ubx/lfrb")
end

function TestIblockOverruns:TestLfrbPartialRejected()
   setup_node("TestLfrbPartialRej", "lfrb")
   check_partial_rejected("ubx/lfrb")
end

function TestIblockOverruns:TestLfdsCyclicOverruns()
   setup_node("TestCyclicOverruns", "lfds_cyclic")
   check_overruns("ubx/lfds_cyclic")
end

function TestIblockOverruns:TestLfdsCyclicAllowPartial()
   setup_node("TestCyclicPartial", "lfds_cyclic")
   check_allow_partial("ubx/lfds_cyclic")
end

function TestIblockOverruns:TestLfdsCyclicPartialRejected()
   setup_node("TestCyclicPartialRej", "lfds_cyclic")
   check_partial_rejected("ubx/lfds_cyclic")
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
