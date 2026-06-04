local luaunit = require("luaunit")
local ubx = require("ubx")
local utils = require("utils")
local bd = require("blockdiagram")
local time = require("time")
local ffi = require("ffi")

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_INFO

local assert_true = luaunit.assert_true
local assert_equals = luaunit.assert_equals
local assert_not_nil = luaunit.assert_not_nil
local assert_not_equals = luaunit.assert_not_equals

TestPtrig = {}

local count_num_trigs = [[
local ubx=require "ubx"

local p_ramp_cnt, p_test_result

function init(b)
   ubx.inport_add(b, "ramp_cnt", "ramp counter in", 0, "uint64_t", 1)
   ubx.outport_add(b, "test_result", "test results", 0, "int", 1)
   p_ramp_cnt = ubx.port_get(b, "ramp_cnt")
   p_test_result = ubx.port_get(b, "test_result")
   return true
end

local trig_cnt = 0
local test_result = 999

function step(b)
   local _, res = p_ramp_cnt:read()
   if trig_cnt ~= res:tolua() then
      test_result = -1
   end

   ubx.port_write(p_test_result, test_result)

   trig_cnt=trig_cnt+1
end

function cleanup(b)
   ubx.port_rm(b, "ramp_cnt")
   ubx.port_rm(b, "test_result")
end
]]

local sys1 = bd.system {
   imports = { "stdtypes", "ptrig", "ramp_uint64", "lfrb", "luablock" },
   blocks = {
      { name="ramp", type="ubx/ramp_uint64" },
      { name="tester", type="ubx/luablock" },
      { name="trig", type="ubx/ptrig" },
   },
   connections = {
      { src="ramp.out", tgt="tester.ramp_cnt" },
   },

   configurations = {
      { name="ramp", config = { start=0, slope=1 } },
      { name="tester", config = { lua_str=count_num_trigs } },
      { name="trig", config = { period = {sec=0, usec=100000 },
				chain0={
				   { b="#ramp" },
				   { b="#tester" } } } },
   },
}

function TestPtrig:TestCountNumTrigs()
   local nd = sys1:launch{ nostart=true, loglevel=LOGLEVEL, nodename='sys1' }
   local p_result = ubx.port_clone_conn(nd:b("tester"), "test_result")
   sys1:startup(nd)
   ubx.clock_mono_sleep(1)
   nd:b("trig"):do_stop()
   local _, res = p_result:read()
   assert_equals(res:tolua(), 999)
   ubx.node_rm(nd)
end


---
--- timing statistics test
---

local dur_test_block_tmpl = [[
local ubx=require "ubx"

function step(b)
   ubx.clock_mono_sleep($SEC, $NSEC)
end
]]

local function gen_dur_test_block(sec, nsec)
   return utils.expand(dur_test_block_tmpl, { SEC=sec, NSEC=nsec})
end

local block_dur_us = {
   ['chain0,tb1'] = 10*1000,
   ['chain0,tb2'] = 50*1000,
   ['chain0,tb3'] = 100*1000,
   ['chain0,#total#'] = 160*1000
}


local sys2 = bd.system {
   imports = { "stdtypes", "ptrig", "lfrb", "luablock" },
   blocks = {
      { name="tb1", type="ubx/luablock" },
      { name="tb2", type="ubx/luablock" },
      { name="tb3", type="ubx/luablock" },
      { name="trig", type="ubx/ptrig" },
   },
   configurations = {
      { name="tb1", config = { lua_str=gen_dur_test_block(0, block_dur_us['chain0,tb1']*1000) } },
      { name="tb2", config = { lua_str=gen_dur_test_block(0, block_dur_us['chain0,tb2']*1000) } },
      { name="tb3", config = { lua_str=gen_dur_test_block(0, block_dur_us['chain0,tb3']*1000) } },
      { name="trig", config = { period = {sec=0, usec=20000 },
				tstats_mode=2,
				tstats_profile_path="./",
				chain0={
				   { b="#tb1" },
				   { b="#tb2" },
				   { b="#tb3" } } } },
   },
}


local min_eps = 0.15 -- 15%: the sleep must have actually happened
local max_eps = 0.50 -- 50%: loose upper bound; OS wakeup latency on SCHED_OTHER
                     -- is non-deterministic so this only catches obviously broken tstats

function TestPtrig:TestTstats()

   local function check_tstat(res)
      local min_us = time.ts2us(res.min)
      local max_us = time.ts2us(res.max)

      assert_true(min_us > block_dur_us[res.id],
		  res.id..
		     ": tstat.min ("..min_us.. " lower than allowed minimal dur ("..
		     block_dur_us[res.id]..")")
      assert_true(max_us < block_dur_us[res.id]*(1+max_eps),
		  res.id..
		     ": tstat.max ("..max_us..") larger than allowed max dur ("..
		     block_dur_us[res.id]*(1+max_eps)..")")
   end

   local nd = sys2:launch{ nostart=true, loglevel=LOGLEVEL, nodename='sys2' }
   local p_tstats = ubx.port_clone_conn(nd:b("trig"), "tstats", 4)

   sys2:startup(nd)
   ubx.clock_mono_sleep(1)
   nd:b("trig"):do_stop()

   while true do
      local cnt, res = p_tstats:read()
      if cnt <= 0 then break end
      check_tstat(res:tolua())
   end

   ubx.node_rm(nd)
end


--
-- trig multichain
--

local sys3 = bd.system {
   imports = { "stdtypes", "trig", "lfrb", "cconst" },
   blocks = {
      { name="const0", type="ubx/cconst" },
      { name="const1", type="ubx/cconst" },
      { name="const2", type="ubx/cconst" },
      { name="const3", type="ubx/cconst" },
      { name="trig0", type="ubx/trig" },
   },

   configurations = {
      { name="const0", config = { type_name="int", value=1000 } },
      { name="const1", config = { type_name="int", value=1001 } },
      { name="const2", config = { type_name="int", value=1002 } },
      { name="const3", config = { type_name="int", value=1003 } },
      {
	 name="trig0", config = {
	    tstats_mode = 2,
	    tstats_profile_path = "./",
	    num_chains = 5,
	    chain0 = { { b="#const0" } },
	    chain1 = { { b="#const1" } },
	    chain2 = { { b="#const2" } },
	    chain3 = { { b="#const3" } },
	    -- empty: chain4 = { }
	 }
      }
   },
}

function TestPtrig:TestMultichainTrig()
   local nd = sys3:launch{ loglevel=LOGLEVEL, nodename='TestTrigMultichain' }

   local p_actchain = ubx.port_clone_conn(nd:b("trig0"), "active_chain")
   local p_const0 = ubx.port_clone_conn(nd:b("const0"), "out")
   local p_const1 = ubx.port_clone_conn(nd:b("const1"), "out")
   local p_const2 = ubx.port_clone_conn(nd:b("const2"), "out")
   local p_const3 = ubx.port_clone_conn(nd:b("const3"), "out")
   local b_trig0 = nd:b("trig0")

   local function rdports()
      local function rd(p)
	 local c, v = p:read()
	 assert_true(c >= 0)
	 if c == 0 then return false end
	 return v:tolua()
      end
      return { rd(p_const0), rd(p_const1), rd(p_const2), rd(p_const3) }
   end

   for _=1,10 do
      b_trig0:do_step();
      assert_equals(rdports(), { 1000, false, false ,false })
   end

   p_actchain:write(1)

   for _=1,10 do
      b_trig0:do_step();
      assert_equals(rdports(), { false, 1001, false ,false })
   end

   p_actchain:write(2)

   for _=1,10 do
      b_trig0:do_step();
      assert_equals(rdports(), { false, false, 1002 ,false })
   end

   -- test the empty chain
   p_actchain:write(4)

   for _=1,10 do
      b_trig0:do_step();
      assert_equals(rdports(), { false, false, false , false })
   end

   p_actchain:write(3)

   for _=1,10 do
      b_trig0:do_step();
      assert_equals(rdports(), { false, false, false , 1003 })
   end

   -- invalid chain
   p_actchain:write(99)

   for _=1,10 do
      b_trig0:do_step();
      assert_equals(rdports(), { false, false, false , 1003 })
   end

   -- invalid chain
   p_actchain:write(-1)

   for _=1,10 do
      b_trig0:do_step();
      assert_equals(rdports(), { false, false, false , 1003 })
   end

   -- back to 0
   p_actchain:write(0)
   for _=1,10 do
      b_trig0:do_step();
      assert_equals(rdports(), { 1000, false, false ,false })
   end

   ubx.node_rm(nd)
end

--
-- ptrig multichain
--
local sys4 = bd.system {
   imports = { "stdtypes", "ptrig", "lfrb", "cconst" },
   blocks = {
      { name="const0", type="ubx/cconst" },
      { name="const1", type="ubx/cconst" },
      { name="const2", type="ubx/cconst" },
      { name="const3", type="ubx/cconst" },
      { name="ptrig0", type="ubx/ptrig" },
   },

   configurations = {
      { name="const0", config = { type_name="int", value=1000 } },
      { name="const1", config = { type_name="int", value=1001 } },
      { name="const2", config = { type_name="int", value=1002 } },
      { name="const3", config = { type_name="int", value=1003 } },
      {
	 name="ptrig0", config = {
	    period = {sec=0, usec=100 },
	    tstats_mode = 2,
	    tstats_profile_path = "./",
	    num_chains = 4,
	    chain0 = { { b="#const0" } },
	    chain1 = { { b="#const1" } },
	    chain2 = { { b="#const2" } },
	    chain3 = { { b="#const3" } }
	 }
      }
   },
}

function TestPtrig:TestMultichainPtrig()
   local nd = sys4:launch{ loglevel=LOGLEVEL, nodename='TestPtrigMultichain' }

   local p_actchain = ubx.port_clone_conn(nd:b("ptrig0"), "active_chain")
   local p_const0 = ubx.port_clone_conn(nd:b("const0"), "out")
   local p_const1 = ubx.port_clone_conn(nd:b("const1"), "out")
   local p_const2 = ubx.port_clone_conn(nd:b("const2"), "out")
   local p_const3 = ubx.port_clone_conn(nd:b("const3"), "out")
   local b_ptrig0 = nd:b("ptrig0")

   local function rdports()
      local function rd(p)
	 local c, v = p:read()
	 assert_true(c >= 0)
	 if c == 0 then return false end
	 return v:tolua()
      end
      while true do
	 local res = { rd(p_const0), rd(p_const1), rd(p_const2), rd(p_const3) }
	 if ( res[1] or res[2] or res[3] or res[4] ) then return res end
      end
   end

   local function switch_chain(id)
      p_actchain:write(id)
      -- clear out old values and ensure that new data is there for
      -- the assert rdports call:
      rdports()
   end

   -- allow ptrig to startup
   assert_equals(rdports(), { 1000, false, false ,false })

   switch_chain(1)
   assert_equals(rdports(), { false, 1001, false ,false })

   switch_chain(2)
   assert_equals(rdports(), { false, false, 1002 ,false })

   switch_chain(3)
   assert_equals(rdports(), { false, false, false , 1003 })

   -- invalid chain
   switch_chain(99)
   assert_equals(rdports(), { false, false, false , 1003 })

   -- invalid chain
   switch_chain(-1)
   assert_equals(rdports(), { false, false, false , 1003 })

   -- back to 0
   switch_chain(0)
   assert_equals(rdports(), { 1000, false, false ,false })

   b_ptrig0:do_stop()

   ubx.node_rm(nd)
end



--
-- period port test
--

local sys5 = bd.system {
   imports = { "stdtypes", "ptrig", "ramp_uint64", "lfrb" },
   blocks = {
      { name="ramp",  type="ubx/ramp_uint64" },
      { name="ptrig", type="ubx/ptrig" },
   },
   configurations = {
      { name="ramp", config = { start=0, slope=1 } },
      { name="ptrig", config = {
	 period = { sec=0, usec=20000 },  -- 20ms / 50 Hz
	 chain0 = { { b="#ramp" } }
      }},
   },
}

function TestPtrig:TestPeriodPort()
   local nd = sys5:launch{ nostart=true, loglevel=LOGLEVEL, nodename='TestPeriodPort' }
   local p_period = ubx.port_clone_conn(nd:b("ptrig"), "period")
   local p_ramp   = ubx.port_clone_conn(nd:b("ramp"), "out", 1)

   sys5:startup(nd)

   -- run at 20ms for 500ms → ~25 steps
   ubx.clock_mono_sleep(0, 500000000)
   local _, v1 = p_ramp:read()
   local steps_fast = v1:tolua()

   -- switch to 500ms period via port
   p_period:write({ sec=0, usec=500000 })

   -- run another 500ms at slow rate → at most 1-2 additional steps
   ubx.clock_mono_sleep(0, 500000000)
   local _, v2 = p_ramp:read()
   local steps_slow = v2:tolua() - steps_fast

   nd:b("ptrig"):do_stop()
   ubx.node_rm(nd)

   assert_true(steps_fast > 10,
	       "expected >10 steps at 20ms period, got " .. steps_fast)
   assert_true(steps_slow < 5,
	       "expected <5 steps at 500ms period, got " .. steps_slow)
end

-- node_rm must stop active (thread-owning) blocks like ptrig before
-- the passive blocks they trigger. Otherwise the trigger thread keeps
-- stepping already stopped blocks ("cblock_step: block not active").
-- The probe block is created before the ptrig (i.e. earlier in hash
-- order) and its stop hook records whether the trigger was already
-- stopped at that point.
local probe_lua_str = [[
local ubx = require("ubx")
local ffi = require("ffi")

function stop(b)
   b = ffi.cast("ubx_block_t*", b)
   local trig = ubx.block_get(b.nd, "trigger")
   local f = assert(io.open("%s", "w"))
   if trig.block_state == ffi.C.BLOCK_STATE_ACTIVE then
      f:write("trigger-still-active")
   else
      f:write("ok")
   end
   f:close()
end
]]

function TestPtrig:TestNodeRmStopsTriggersFirst()
   local resfile = os.tmpname()

   local sys6 = bd.system {
      imports = { "stdtypes", "ptrig", "luablock" },
      blocks = {
	 -- probe first: under hash-order stopping it would be
	 -- stopped before the trigger
	 { name="probe",   type="ubx/luablock" },
	 { name="trigger", type="ubx/ptrig" },
      },
      configurations = {
	 { name="probe", config = { lua_str = string.format(probe_lua_str, resfile) } },
	 { name="trigger", config = {
	      period = { sec=0, usec=1000 },
	      chain0 = { { b="#probe" } }
	 }},
      },
   }

   local nd = sys6:launch{ loglevel=LOGLEVEL, nodename='TestNodeRmStopsTriggersFirst' }
   ubx.clock_mono_sleep(0, 50000000) -- 50ms
   ubx.node_rm(nd)

   local f = assert(io.open(resfile, "r"))
   local res = f:read("*a")
   f:close()
   os.remove(resfile)

   assert_equals(res, "ok")
end


--
-- SCHED_DEADLINE tests
--

local DEADLINE_ND_OPTS = { loglevel = ffi.C.UBX_LOGLEVEL_WARN }

local function make_deadline_node(name)
   local nd = ubx.node_create(name, DEADLINE_ND_OPTS)
   ubx.load_module(nd, "stdtypes")
   ubx.load_module(nd, "ptrig")
   return nd
end

-- Config-validation tests: ptrig_handle_config fails before pthread_create,
-- so no thread is created and no root privilege is needed.

function TestPtrig:TestDeadlineConfigMissingParam()
   local nd = make_deadline_node("dl_cfg_missing")
   local b = ubx.block_create(nd, "ubx/ptrig", "pt",
      { period = {sec=0, usec=1000}, sched_policy = "SCHED_DEADLINE" })
   assert_not_nil(b)
   assert_not_equals(ubx.block_tostate(b, 'inactive'), 0,
      "init should fail without sched_deadline config")
   assert_equals(b.block_state, ffi.C.BLOCK_STATE_PREINIT)
   ubx.node_rm(nd)
end

function TestPtrig:TestDeadlineConfigZeroRuntime()
   local nd = make_deadline_node("dl_cfg_zero_rt")
   local b = ubx.block_create(nd, "ubx/ptrig", "pt", {
      period         = { sec=0, usec=1000 },
      sched_policy   = "SCHED_DEADLINE",
      sched_deadline = { runtime_ns=0, deadline_ns=0, period_ns=0 },
   })
   assert_not_nil(b)
   assert_not_equals(ubx.block_tostate(b, 'inactive'), 0,
      "init should fail with runtime_ns=0")
   assert_equals(b.block_state, ffi.C.BLOCK_STATE_PREINIT)
   ubx.node_rm(nd)
end

function TestPtrig:TestDeadlineConfigConstraintViolation()
   local nd = make_deadline_node("dl_cfg_constraint")
   local b = ubx.block_create(nd, "ubx/ptrig", "pt", {
      period         = { sec=0, usec=1000 },
      sched_policy   = "SCHED_DEADLINE",
      sched_deadline = { runtime_ns=900000, deadline_ns=500000, period_ns=1000000 },
   })
   assert_not_nil(b)
   assert_not_equals(ubx.block_tostate(b, 'inactive'), 0,
      "init should fail when runtime_ns > deadline_ns")
   assert_equals(b.block_state, ffi.C.BLOCK_STATE_PREINIT)
   ubx.node_rm(nd)
end

-- SCHED_DEADLINE is rejected in non-initial user namespaces regardless of
-- CAP_SYS_NICE, so rootless containers (podman/docker) can't run this test.
-- The init userns has uid_map "0 0 4294967295"; anything else is a child ns.
local function in_root_userns()
   local f = io.open("/proc/self/uid_map", "r")
   if not f then return true end
   local line = f:read("*l"); f:close()
   local inner, outer, len = (line or ""):match("^%s*(%d+)%s+(%d+)%s+(%d+)")
   return inner == "0" and outer == "0"
          and tonumber(len) and tonumber(len) > 1000000
end

-- Functional test: ptrig with SCHED_DEADLINE config runs and steps correctly.
-- Requires CAP_SYS_NICE (run via run_tests.sh which grants it through capsh).
local sys_dl = bd.system {
   imports = { "stdtypes", "ptrig", "ramp_uint64", "lfrb" },
   blocks = {
      { name="ramp",  type="ubx/ramp_uint64" },
      { name="ptrig", type="ubx/ptrig" },
   },
   configurations = {
      { name="ramp", config = { start=0, slope=1 } },
      { name="ptrig", config = {
         period         = { sec=0, usec=10000 },  -- 10ms / 100Hz
         sched_policy   = "SCHED_DEADLINE",
         sched_deadline = { runtime_ns=5000000 },  -- 5ms WCET; deadline/period from 'period'
         chain0         = { { b="#ramp" } },
      }},
   },
}

function TestPtrig:TestDeadlineRuns()
   luaunit.skipIf(not in_root_userns(),
      "SCHED_DEADLINE not available in rootless user namespace")
   local nd = sys_dl:launch{ nostart=true, loglevel=LOGLEVEL, nodename='TestDeadlineRuns' }
   local p_ramp = ubx.port_clone_conn(nd:b("ramp"), "out", 1)

   sys_dl:startup(nd)
   ubx.clock_mono_sleep(0, 500000000)  -- 500ms
   nd:b("ptrig"):do_stop()

   local cnt, val = p_ramp:read()
   local v = cnt > 0 and val:tolua() or nil
   ubx.node_rm(nd)

   assert_true(cnt > 0, "no ramp value received")
   assert_true(v > 10,
      "expected >10 steps in 500ms at 10ms period, got " .. tostring(v))
end

if not _RUNNER then os.exit( luaunit.LuaUnit.run() ) end
