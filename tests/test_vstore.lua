--
-- Test vstore (single-slot value store) as iblock
--

local lu = require("luaunit")
local ubx = require("ubx")
local ffi = require("ffi")

local assert_equals = lu.assert_equals
local assert_not_nil = lu.assert_not_nil

-- CRIT: the type-mismatch cases below log an expected ubx_err
local LOGLEVEL = ffi.C.UBX_LOGLEVEL_CRIT

local nd

TestVstore = {}

function TestVstore:teardown()
   if nd then ubx.node_rm(nd) end
   nd = nil
end

local function setup_node(name)
   nd = ubx.node_create(name, { loglevel = LOGLEVEL })
   ubx.load_module(nd, "stdtypes")
   ubx.load_module(nd, "lfrb")	-- for port_clone_conn
   ubx.load_module(nd, "vstore")
   ubx.ffi_load_types(nd)
end

local function create_ib(config)
   local ib = ubx.block_create(nd, "ubx/vstore", "ib1")
   assert_not_nil(ib)
   ubx.do_configure(ib, config)
   assert_equals(ubx.block_tostate(ib, 'active'), 0)
   return ib
end

--- nothing written yet: the reader must not get the zeroed buffer
function TestVstore:TestEmptyBeforeFirstWrite()
   setup_node("TestVstoreEmpty")
   local ib = create_ib({ type_name = "int" })
   local rdat = ubx.data_alloc(nd, "int", 1)

   assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 0)
end

--- a read consumes: the same contract as lfrb's empty usedq
function TestVstore:TestReadConsumes()
   setup_node("TestVstoreConsume")
   local ib = create_ib({ type_name = "int" })
   local wdat = ubx.data_alloc(nd, "int", 1)
   local rdat = ubx.data_alloc(nd, "int", 1)

   ubx.data_set(wdat, 42)
   ubx.interaction_write(ib, wdat)

   assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 1)
   assert_equals(rdat:tolua(), 42)

   -- consumed: nothing held any more
   assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 0)
end

--- the newest write wins and is the one handed to the reader
function TestVstore:TestWriteWriteRead()
   setup_node("TestVstoreWWR")
   local ib = create_ib({ type_name = "int" })
   local wdat = ubx.data_alloc(nd, "int", 1)
   local rdat = ubx.data_alloc(nd, "int", 1)

   ubx.data_set(wdat, 1)
   ubx.interaction_write(ib, wdat)
   ubx.data_set(wdat, 2)
   ubx.interaction_write(ib, wdat)

   assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 1)
   assert_equals(rdat:tolua(), 2)
   assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 0)
end

--- a write landing before the previous value was read is the block
--- reporting that its same-thread precondition does not hold
function TestVstore:TestOverwritesReported()
   setup_node("TestVstoreOverwrites")
   local ib = create_ib({ type_name = "int" })
   local po = ubx.port_clone_conn(ib, "overwrites", nil, 1, 7, 0)
   local wdat = ubx.data_alloc(nd, "int", 1)
   local rdat = ubx.data_alloc(nd, "int", 1)

   -- write, read, write, read: nothing is ever overwritten unread
   for v = 1, 3 do
      ubx.data_set(wdat, v)
      ubx.interaction_write(ib, wdat)
      assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 1)
   end
   assert_equals(tonumber((po:read())), 0)	-- output only upon change

   -- now overwrite an unread value twice
   for v = 1, 3 do
      ubx.data_set(wdat, v)
      ubx.interaction_write(ib, wdat)
   end

   local len, val = po:read()
   assert_equals(tonumber(len), 1)
   assert_equals(tonumber(val:tolua()), 2)
end

--- the stored length is what was written, not what the buffer holds, so
--- a short write does not hand the reader the stale tail
function TestVstore:TestShortWrite()
   setup_node("TestVstoreShort")
   local ib = create_ib({ type_name = "int", data_len = 4 })
   local wdat = ubx.data_alloc(nd, "int", 4)
   local wshort = ubx.data_alloc(nd, "int", 2)
   local rdat = ubx.data_alloc(nd, "int", 4)

   ubx.data_set(wdat, { 1, 2, 3, 4 })
   ubx.interaction_write(ib, wdat)
   assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 4)

   ubx.data_set(wshort, { 42, 43 })
   ubx.interaction_write(ib, wshort)
   assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 2)

   -- a full-width write afterwards goes back to the full length
   ubx.data_set(wdat, { 9, 9, 9, 9 })
   ubx.interaction_write(ib, wdat)
   assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 4)
end

--- array data round-trips
function TestVstore:TestVectorData()
   setup_node("TestVstoreVec")
   local ib = create_ib({ type_name = "double", data_len = 3 })
   local wdat = ubx.data_alloc(nd, "double", 3)
   local rdat = ubx.data_alloc(nd, "double", 3)

   ubx.data_set(wdat, { 1.5, -2.5, 3.5 })
   ubx.interaction_write(ib, wdat)

   assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 3)
   assert_equals(rdat:tolua(), { 1.5, -2.5, 3.5 })
end

--- a read of the wrong type is refused rather than reinterpreted
function TestVstore:TestTypeMismatch()
   setup_node("TestVstoreType")
   local ib = create_ib({ type_name = "int" })
   local wdat = ubx.data_alloc(nd, "int", 1)
   local rwrong = ubx.data_alloc(nd, "double", 1)

   ubx.data_set(wdat, 7)
   ubx.interaction_write(ib, wdat)

   -- call read() directly: the lua wrapper turns a negative return into an
   -- error rather than handing it back
   assert_equals(tonumber(ib.read(ib, rwrong)), tonumber(ffi.C.EINVALID_TYPE))

   -- the held value is untouched by the refused read
   local rdat = ubx.data_alloc(nd, "int", 1)
   assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 1)
   assert_equals(rdat:tolua(), 7)
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
