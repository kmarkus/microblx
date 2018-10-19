local luaunit=require("luaunit")
local ffi=require("ffi")
local ubx=require("ubx")
local utils=require("utils")
local bd = require("blockdiagram")

local assert_equals = luaunit.assert_equals

local count_num_trigs = [[
local ubx=require "ubx"

local ramp_cnt, test_result

function init(b)
   ubx.inport_add(b, "ramp_cnt", "ramp counter in", "uint64_t", 1)
   ubx.outport_add(b, "test_result", "test results", "int", 1)
   p_ramp_cnt = ubx.port_get(b, "ramp_cnt")
   p_test_result = ubx.port_get(b, "test_result")
   return true
end

local trig_cnt = 0
local test_result = 999

function step(block)
   trig_cnt=trig_cnt+1

   local n, res = p_ramp_cnt:read()
   if trig_cnt ~= res:tolua() then
      test_result = -1
   end

   ubx.port_write(p_test_result, test_result)
end

function cleanup(block)
   ubx.port_rm(block, "ramp_cnt")
   ubx.port_rm(block, "tstats")
end
]]

local sys1 = bd.system {
   imports = { "stdtypes", "ptrig", "ramp_uint64", "lfds_cyclic", "luablock" },
   blocks = {
      { name="ramp", type="ramp_uint64" },
      { name="tester", type="lua/luablock" },
      { name="trig", type="std_triggers/ptrig" },
   },
   connections = {
      { src="ramp.out", tgt="tester.ramp_cnt" },
   },

   configurations = {
      { name="ramp", config = { start=0, slope=1 } },
      { name="tester", config = { lua_str=count_num_trigs } },
      { name="trig", config = { period = {sec=0, usec=100000 },
				trig_blocks={
				   { b="#ramp", num_steps=1, measure=0 },
				   { b="#tester", num_steps=1, measure=0 } } } },
   },
}

function test_launch()
   local ni = sys1:launch{ nostart=true, verbose=false }
   local p_result = ubx.port_clone_conn(ni:b("tester"), "test_result")
   sys1:startup(ni)
   ubx.clock_mono_sleep(1)
   ni:b("trig"):stop()
   local _, res = p_result:read()
   assert_equals(res:tolua(), 999)
end

os.exit( luaunit.LuaUnit.run() )
