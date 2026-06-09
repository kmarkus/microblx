--
-- Test the mqueue iblock
--

local lu = require("luaunit")
local ubx = require("ubx")
local ffi = require("ffi")

local assert_equals = lu.assert_equals
local assert_not_nil = lu.assert_not_nil

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_WARN

-- unique id to avoid clashing with stale queues from aborted runs
local MQ_ID = "testmq" .. tostring(os.time())

local nd

TestMqueue = {}

function TestMqueue:teardown()
   if nd then ubx.node_rm(nd) end
   nd = nil
end

local function create_mq(name, config)
   local mq = ubx.block_create(nd, "ubx/mqueue", name)
   assert_not_nil(mq)
   ubx.do_configure(mq, config)
   assert_equals(ubx.block_tostate(mq, 'active'), 0)
   return mq
end

--- scalar roundtrip and empty queue read
function TestMqueue:TestScalarRoundtrip()
   nd = ubx.node_create("TestMqScalar", { loglevel = LOGLEVEL })
   ubx.load_module(nd, "stdtypes")
   ubx.load_module(nd, "mqueue")
   ubx.ffi_load_types(nd)

   local mq = create_mq("mq1", {
      mq_id = MQ_ID .. "s", type_name = "int", buffer_len = 4 })

   local wdat = ubx.data_alloc(nd, "int", 1)
   local rdat = ubx.data_alloc(nd, "int", 1)

   -- empty queue
   assert_equals(tonumber(ubx.interaction_read(mq, rdat)), 0)

   ubx.data_set(wdat, 42)
   ubx.interaction_write(mq, wdat)

   assert_equals(tonumber(ubx.interaction_read(mq, rdat)), 1)
   assert_equals(rdat:tolua(), 42)

   -- queue drained again
   assert_equals(tonumber(ubx.interaction_read(mq, rdat)), 0)
end

--- array data roundtrip
function TestMqueue:TestArrayRoundtrip()
   local DATA_LEN = 3

   nd = ubx.node_create("TestMqArray", { loglevel = LOGLEVEL })
   ubx.load_module(nd, "stdtypes")
   ubx.load_module(nd, "mqueue")
   ubx.ffi_load_types(nd)

   local mq = create_mq("mq1", {
      mq_id = MQ_ID .. "a", type_name = "double",
      data_len = DATA_LEN, buffer_len = 2 })

   local wdat = ubx.data_alloc(nd, "double", DATA_LEN)
   local rdat = ubx.data_alloc(nd, "double", DATA_LEN)

   ubx.data_set(wdat, { 1.5, -2.5, 99.0 })
   ubx.interaction_write(mq, wdat)

   assert_equals(tonumber(ubx.interaction_read(mq, rdat)), DATA_LEN)
   assert_equals(rdat:tolua(), { 1.5, -2.5, 99.0 })
end

--- FIFO order and non-blocking overflow behavior
function TestMqueue:TestFifoOverflow()
   local BUFLEN = 4

   nd = ubx.node_create("TestMqFifo", { loglevel = LOGLEVEL })
   ubx.load_module(nd, "stdtypes")
   ubx.load_module(nd, "mqueue")
   ubx.ffi_load_types(nd)

   local mq = create_mq("mq1", {
      mq_id = MQ_ID .. "f", type_name = "int", buffer_len = BUFLEN })

   local wdat = ubx.data_alloc(nd, "int", 1)
   local rdat = ubx.data_alloc(nd, "int", 1)

   -- write BUFLEN+2: the surplus is dropped (non-blocking)
   for v = 1, BUFLEN + 2 do
      ubx.data_set(wdat, v)
      ubx.interaction_write(mq, wdat)
   end

   -- read back the first BUFLEN values in order
   for v = 1, BUFLEN do
      assert_equals(tonumber(ubx.interaction_read(mq, rdat)), 1)
      assert_equals(rdat:tolua(), v)
   end

   assert_equals(tonumber(ubx.interaction_read(mq, rdat)), 0)
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
