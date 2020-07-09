local luaunit = require("luaunit")
local ubx = require("ubx")
local utils = require("utils")
local bd = require("blockdiagram")
local time = require("time")
local ffi = require("ffi")

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_INFO

local assert_true = luaunit.assert_true
local assert_equals = luaunit.assert_equals
local assert_true = luaunit.assert_true

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
   imports = { "stdtypes", "ptrig", "ramp_uint64", "lfds_cyclic", "luablock" },
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
   imports = { "stdtypes", "ptrig", "lfds_cyclic", "luablock" },
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


local eps = 0.15 -- 15%

function TestPtrig:TestTstats()

   local function check_tstat(res)
      local min_us = time.ts2us(res.min)
      local max_us = time.ts2us(res.max)

      assert_true(min_us > block_dur_us[res.id],
		  res.id..
		     ": tstat.min ("..min_us.. " lower than allowed minimal dur ("..
		     block_dur_us[res.id]..")")
      assert_true(max_us < block_dur_us[res.id]*(1+eps),
		  res.id..
		     ": tstat.max ("..max_us..") larger than allowed max dur ("..
		     block_dur_us[res.id]*(1+eps)..")")
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
   imports = { "stdtypes", "trig", "lfds_cyclic", "cconst" },
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
   imports = { "stdtypes", "ptrig", "lfds_cyclic", "cconst" },
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



os.exit( luaunit.LuaUnit.run() )
