--
-- Test the sync_req/sync_ack ports of trig and ptrig
--

local lu = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")
local ffi = require("ffi")

local assert_equals = lu.assert_equals
local assert_not_nil = lu.assert_not_nil

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_WARN

local nd

TestTrigSync = {}

function TestTrigSync:teardown()
   if nd then ubx.node_rm(nd) end
   nd = nil
end

local function mksys(trigtype, trigcfg)
   trigcfg.chain0 = { { b = "#r1" } }
   return bd.system {
      imports = { "stdtypes", "trig", "ptrig", "lfrb", "ramp" },
      blocks = {
	 { name = "r1", type = "ubx/ramp" },
	 { name = "trig", type = trigtype },
      },
      configurations = {
	 { name = "r1", config = { type_name = "uint64_t", start = 0, slope = 1 } },
	 { name = "trig", config = trigcfg },
      },
   }
end

local function steps(name) return tonumber(nd:b(name).stat_num_steps) end

local function ack(p)
   local len, val = p:read()
   if len <= 0 then return nil end
   return tonumber(val:tolua())
end

--- unconnected sync_req: every step steps the chain
function TestTrigSync:TestUnconnected()
   nd = mksys("ubx/trig", {}):launch{ nodename = "TestSyncUnconn", loglevel = LOGLEVEL }
   assert_not_nil(nd)

   for _ = 1, 5 do ubx.cblock_step(nd:b("trig")) end
   assert_equals(steps("r1"), 5)
end

--- connected: one chain step per new (greater) value, sync_ack echoes it
function TestTrigSync:TestTrig()
   local sys = mksys("ubx/trig", {})
   nd = sys:launch{ nodename = "TestSyncTrig", loglevel = LOGLEVEL, nostart = true }
   local trig = nd:b("trig")
   local p_req = ubx.port_clone_conn(trig, "sync_req")
   local p_ack = ubx.port_clone_conn(trig, "sync_ack")
   sys:startup(nd)

   -- no data: no step
   for _ = 1, 3 do ubx.cblock_step(trig) end
   assert_equals(steps("r1"), 0)
   assert_equals(ack(p_ack), nil)

   -- each new value steps once, a repeat of the step does not
   for v = 1, 3 do
      p_req:write(v)
      ubx.cblock_step(trig)
      ubx.cblock_step(trig)
      assert_equals(steps("r1"), v)
      assert_equals(ack(p_ack), v)
   end

   -- an old or equal value is skipped
   p_req:write(3)
   ubx.cblock_step(trig)
   p_req:write(2)
   ubx.cblock_step(trig)
   assert_equals(steps("r1"), 3)
   assert_equals(ack(p_ack), nil)

   -- a jump by 2: one step, no catch-up
   p_req:write(5)
   ubx.cblock_step(trig)
   ubx.cblock_step(trig)
   assert_equals(steps("r1"), 4)
   assert_equals(ack(p_ack), 5)

   -- a restart forgets the last served value, without replaying it
   ubx.block_tostate(trig, 'inactive')
   ubx.block_tostate(trig, 'active')
   ubx.cblock_step(trig)
   assert_equals(steps("r1"), 4)
   p_req:write(1)
   ubx.cblock_step(trig)
   assert_equals(steps("r1"), 5)
   assert_equals(ack(p_ack), 1)

   -- a failed chain step is neither acked, nor retried
   ubx.block_tostate(nd:b("r1"), 'inactive')
   p_req:write(6)
   ubx.cblock_step(trig)
   assert_equals(ack(p_ack), nil)
   ubx.block_tostate(nd:b("r1"), 'active')
   ubx.cblock_step(trig)
   assert_equals(steps("r1"), 5)
   assert_equals(ack(p_ack), nil)
end

--- sync_ack unconnected: the chain is still gated by sync_req
function TestTrigSync:TestNoAck()
   local sys = mksys("ubx/trig", {})
   nd = sys:launch{ nodename = "TestSyncNoAck", loglevel = LOGLEVEL, nostart = true }
   local trig = nd:b("trig")
   local p_req = ubx.port_clone_conn(trig, "sync_req")
   sys:startup(nd)

   p_req:write(7)
   ubx.cblock_step(trig)
   ubx.cblock_step(trig)
   assert_equals(steps("r1"), 1)
end

local function wait_steps(n)
   for _ = 1, 100 do
      if steps("r1") >= n then break end
      ubx.clock_mono_sleep(0, 1000000)
   end
   ubx.clock_mono_sleep(0, 20000000)	-- would catch extra steps
end

--- free-running ptrig polls sync_req; skipped passes are no tstats
function TestTrigSync:TestPtrigFreeRun()
   local sys = mksys("ubx/ptrig", { period_ns = 0, tstats_mode = 1 })
   nd = sys:launch{ nodename = "TestSyncPtrigFree", loglevel = LOGLEVEL, nostart = true }
   local trig = nd:b("trig")
   local p_req = ubx.port_clone_conn(trig, "sync_req")
   local p_ack = ubx.port_clone_conn(trig, "sync_ack", 4)
   local p_tstats = ubx.port_clone_conn(trig, "tstats", 4)
   sys:startup(nd)

   ubx.clock_mono_sleep(0, 20000000)
   assert_equals(steps("r1"), 0)

   p_req:write(1)
   wait_steps(1)
   assert_equals(steps("r1"), 1)
   assert_equals(ack(p_ack), 1)

   p_req:write(3)
   wait_steps(2)
   assert_equals(steps("r1"), 2)
   assert_equals(ack(p_ack), 3)

   assert_equals(trig:do_stop(), 0)

   local len, ts = p_tstats:read()
   assert_equals(tonumber(len), 1)
   assert_equals(tonumber(ts:tolua().cnt), 2)
end

--- periodic ptrig: skipped periods are not overruns
function TestTrigSync:TestPtrigPeriodic()
   local sys = mksys("ubx/ptrig", { period_ns = 1000000 })
   nd = sys:launch{ nodename = "TestSyncPtrigPer", loglevel = LOGLEVEL, nostart = true }
   local trig = nd:b("trig")
   local p_req = ubx.port_clone_conn(trig, "sync_req")
   local p_ovr = ubx.port_clone_conn(trig, "overrun_cnt")
   sys:startup(nd)

   ubx.clock_mono_sleep(0, 50000000)
   assert_equals(steps("r1"), 0)

   p_req:write(10)
   wait_steps(1)
   assert_equals(steps("r1"), 1)

   assert_equals(trig:do_stop(), 0)
   -- skipped periods would count ~50 here; allow a few real
   -- overruns from scheduling latency on a loaded host
   lu.assert_true(tonumber((p_ovr:read())) <= 3, "skipped periods counted as overruns")
end

--- autostop_steps counts chain steps, not passes
function TestTrigSync:TestPtrigAutostop()
   local sys = mksys("ubx/ptrig", { period_ns = 0, autostop_steps = 2 })
   nd = sys:launch{ nodename = "TestSyncPtrigAuto", loglevel = LOGLEVEL, nostart = true }
   local trig = nd:b("trig")
   local p_req = ubx.port_clone_conn(trig, "sync_req")
   sys:startup(nd)

   ubx.clock_mono_sleep(0, 20000000)
   p_req:write(1)
   wait_steps(1)
   assert_equals(tonumber(trig.block_state), tonumber(ffi.C.BLOCK_STATE_ACTIVE))

   p_req:write(2)
   wait_steps(2)
   assert_equals(steps("r1"), 2)
   assert_equals(tonumber(trig.block_state), tonumber(ffi.C.BLOCK_STATE_INACTIVE))
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
