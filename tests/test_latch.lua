--
-- Test latch (single-writer multi-reader latest-value store) as iblock
--

local lu = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")
local ffi = require("ffi")

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_WARN

local ni

TestLatch = {}

function TestLatch:setup()
   ubx.reset_block_uid()
end

function TestLatch:teardown()
   if ni then ubx.node_rm(ni) end
   ni = nil
end

local function mksys(nodename, cfg)
   local sys = bd.system {
      imports = { "stdtypes", "latch", "lfrb", "saturation" },
      blocks = {
	 { name = "sat1", type = "ubx/saturation" },
	 { name = "sat2", type = "ubx/saturation" },
      },
      configurations = {
	 { name = "sat1", config = cfg.sat1 },
	 { name = "sat2", config = cfg.sat2 },
      },
      connections = {
	 { src = "sat1.out", tgt = "sat2.in", type = "ubx/latch" },
      },
   }

   lu.assert_equals(sys:validate(false), 0)
   local n = sys:launch({ nodename = nodename, loglevel = LOGLEVEL })
   lu.assert_not_nil(n)
   return n
end

local SAT1 = { type="double", lower_limits = -10, upper_limits = 10 }
local SAT2 = { type="double", lower_limits = -5, upper_limits = 5 }

--- Basic connection through a latch
function TestLatch:TestSimpleConnection()
   ni = mksys("TestLatch", { sat1 = SAT1, sat2 = SAT2 })

   local sat1, sat2 = ni:b("sat1"), ni:b("sat2")
   local pin1 = ubx.port_clone_conn(sat1, "in", 1, nil, 7, 0)
   local pout2 = ubx.port_clone_conn(sat2, "out", nil, 1, 7, 0)

   pin1:write(3.0)
   sat1:do_step()
   sat2:do_step()
   local len, val = pout2:read()
   lu.assert_equals(tonumber(len), 1)
   lu.assert_equals(val:tolua(), 3.0)

   -- clipped by sat1 to 10, then by sat2 to 5
   pin1:write(100.0)
   sat1:do_step()
   sat2:do_step()
   len, val = pout2:read()
   lu.assert_equals(tonumber(len), 1)
   lu.assert_equals(val:tolua(), 5.0)
end

--- The defining property: a read does not consume, so a reader that steps
--- without a new write still sees the last value. lfrb and vstore return
--- no-data here.
function TestLatch:TestRepeatsLastValue()
   ni = mksys("TestLatchRepeat", { sat1 = SAT1, sat2 = SAT2 })

   local sat1, sat2 = ni:b("sat1"), ni:b("sat2")
   local pin1 = ubx.port_clone_conn(sat1, "in", 1, nil, 7, 0)
   local pout2 = ubx.port_clone_conn(sat2, "out", nil, 1, 7, 0)

   pin1:write(2.0)
   sat1:do_step()		-- one write into the latch

   for _ = 1, 5 do		-- five reads of it
      sat2:do_step()
      local len, val = pout2:read()
      lu.assert_equals(tonumber(len), 1)
      lu.assert_equals(val:tolua(), 2.0)
   end
end

--- Before the first write the latch reads as no-data, so the reader never
--- sees the zeroed buffer as if it were a value.
function TestLatch:TestEmptyBeforeFirstWrite()
   ni = mksys("TestLatchEmpty", { sat1 = SAT1, sat2 = SAT2 })

   local sat2 = ni:b("sat2")
   local pout2 = ubx.port_clone_conn(sat2, "out", nil, 1, 7, 0)

   sat2:do_step()		-- sat1 never stepped: latch is empty
   local len = pout2:read()
   lu.assert_equals(tonumber(len), 0)
end

--- Two readers on one latch both see every value. Against lfrb the first
--- reader would consume the sample and the second would get nothing. The
--- latch is declared as a named block so both connections share one instance.
function TestLatch:TestMultipleReaders()
   local sys = bd.system {
      imports = { "stdtypes", "latch", "lfrb", "saturation" },
      blocks = {
	 { name = "l1", type = "ubx/latch" },
	 { name = "sat1", type = "ubx/saturation" },
	 { name = "sat2", type = "ubx/saturation" },
	 { name = "sat3", type = "ubx/saturation" },
      },
      configurations = {
	 { name = "l1", config = { type_name = "double", data_len = 1 } },
	 { name = "sat1", config = SAT1 },
	 { name = "sat2", config = SAT2 },
	 { name = "sat3", config = SAT2 },
      },
      connections = {
	 { src = "sat1.out", tgt = "l1" },
	 { src = "l1", tgt = "sat2.in" },
	 { src = "l1", tgt = "sat3.in" },
      },
   }

   lu.assert_equals(sys:validate(false), 0)
   ni = sys:launch({ nodename = "TestLatchMR", loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)

   local sat1, sat2, sat3 = ni:b("sat1"), ni:b("sat2"), ni:b("sat3")
   local pin1 = ubx.port_clone_conn(sat1, "in", 1, nil, 7, 0)
   local pout2 = ubx.port_clone_conn(sat2, "out", nil, 1, 7, 0)
   local pout3 = ubx.port_clone_conn(sat3, "out", nil, 1, 7, 0)

   pin1:write(4.0)
   sat1:do_step()
   sat2:do_step()
   sat3:do_step()

   local len2, val2 = pout2:read()
   local len3, val3 = pout3:read()
   lu.assert_equals(tonumber(len2), 1)
   lu.assert_equals(tonumber(len3), 1)
   lu.assert_equals(val2:tolua(), 4.0)
   lu.assert_equals(val3:tolua(), 4.0)
end

--- Array data, and a repeat of it
function TestLatch:TestVectorData()
   local DATA_LEN = 3

   ni = mksys("TestLatchVec", {
      sat1 = { type="double", data_len = DATA_LEN,
	       lower_limits = { -10, -10, -10 }, upper_limits = { 10, 10, 10 } },
      sat2 = { type="double", data_len = DATA_LEN,
	       lower_limits = { -5, -5, -5 }, upper_limits = { 5, 5, 5 } },
   })

   local sat1, sat2 = ni:b("sat1"), ni:b("sat2")
   local pin1 = ubx.port_clone_conn(sat1, "in", 1, nil, 7, 0)
   local pout2 = ubx.port_clone_conn(sat2, "out", nil, 1, 7, 0)

   pin1:write({ 3.0, -100.0, 7.0 })
   sat1:do_step()

   for _ = 1, 3 do
      sat2:do_step()
      local len, val = pout2:read()
      lu.assert_equals(tonumber(len), DATA_LEN)
      -- sat1: {3, -10, 7}, sat2: {3, -5, 5}
      lu.assert_equals(val:tolua(), { 3.0, -5.0, 5.0 })
   end
end

--- The cases below drive the iblock directly rather than through a
--- connection: they are about exact read lengths and return values, which
--- port_clone_conn cannot express.

local function setup_ib(name, config)
   ni = ubx.node_create(name, { loglevel = ffi.C.UBX_LOGLEVEL_CRIT })
   ubx.load_module(ni, "stdtypes")
   ubx.load_module(ni, "latch")
   ubx.ffi_load_types(ni)

   local ib = ubx.block_create(ni, "ubx/latch", "ib1")
   lu.assert_not_nil(ib)
   ubx.do_configure(ib, config)
   lu.assert_equals(ubx.block_tostate(ib, 'active'), 0)
   return ib
end

--- two writes before a read: the second wins
function TestLatch:TestWriteWriteRead()
   local ib = setup_ib("TestLatchWWR", { type_name = "int" })
   local wdat = ubx.data_alloc(ni, "int", 1)
   local rdat = ubx.data_alloc(ni, "int", 1)

   ubx.data_set(wdat, 1)
   ubx.interaction_write(ib, wdat)
   ubx.data_set(wdat, 2)
   ubx.interaction_write(ib, wdat)

   lu.assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 1)
   lu.assert_equals(rdat:tolua(), 2)
   -- still held: a read does not consume
   lu.assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 1)
   lu.assert_equals(rdat:tolua(), 2)
end

--- the stored length is what was written, not what the buffer holds, so a
--- short write does not hand the reader the stale tail
function TestLatch:TestShortWrite()
   local ib = setup_ib("TestLatchShort", { type_name = "int", data_len = 4 })
   local wdat = ubx.data_alloc(ni, "int", 4)
   local wshort = ubx.data_alloc(ni, "int", 2)
   local rdat = ubx.data_alloc(ni, "int", 4)

   ubx.data_set(wdat, { 1, 2, 3, 4 })
   ubx.interaction_write(ib, wdat)
   lu.assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 4)

   ubx.data_set(wshort, { 42, 43 })
   ubx.interaction_write(ib, wshort)
   lu.assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 2)
   -- the repeat keeps the short length too
   lu.assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 2)

   ubx.data_set(wdat, { 9, 9, 9, 9 })
   ubx.interaction_write(ib, wdat)
   lu.assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 4)
end

--- a read of the wrong type is refused rather than reinterpreted
function TestLatch:TestTypeMismatch()
   local ib = setup_ib("TestLatchType", { type_name = "int" })
   local wdat = ubx.data_alloc(ni, "int", 1)
   local rwrong = ubx.data_alloc(ni, "double", 1)

   ubx.data_set(wdat, 7)
   ubx.interaction_write(ib, wdat)

   -- call read() directly: the lua wrapper turns a negative return into an
   -- error rather than handing it back
   lu.assert_equals(tonumber(ib.read(ib, rwrong)), tonumber(ffi.C.EINVALID_TYPE))

   -- the held value is untouched by the refused read
   local rdat = ubx.data_alloc(ni, "int", 1)
   lu.assert_equals(tonumber(ubx.interaction_read(ib, rdat)), 1)
   lu.assert_equals(rdat:tolua(), 7)
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
