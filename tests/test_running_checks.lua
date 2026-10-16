-- stopping blocks used by running blocks

local lu = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")
local ffi = require("ffi")

local assert_equals = lu.assert_equals
local assert_true = lu.assert_true
local assert_false = lu.assert_false

local EWRONG_STATE = ffi.C.EWRONG_STATE

local sink = [[
local ubx = require("ubx")
function init(b)
   ubx.inport_add(b, "in", "", 0, "uint64_t", 1)
   ubx.inport_add(b, "in2", "", 0, "uint64_t", 1)
   return true
end
function cleanup(b) ubx.port_rm(b, "in"); ubx.port_rm(b, "in2") end
]]

local sys = bd.system {
   imports = { "stdtypes", "ptrig", "trig", "ramp_uint64", "lfrb", "luablock" },
   blocks = {
      { name="ramp", type="ubx/ramp_uint64" },
      { name="sink", type="ubx/luablock" },
      { name="inner", type="ubx/trig" },
      { name="trig", type="ubx/ptrig" },
   },
   connections = {
      { src="ramp.out", tgt="sink.in" },
   },
   configurations = {
      { name="ramp", config = { start=0, slope=1 } },
      { name="sink", config = { lua_str=sink } },
      { name="inner", config = { chain0={ { b="#sink" } } } },
      { name="trig", config = { period = {sec=0, usec=1000 },
				chain0={ { b="#ramp" }, { b="#inner" } } } },
   },
}

local nd, ib

TestRunningChecks = {}

function TestRunningChecks:setup()
   nd = sys:launch{ nostart=true, loglevel=ffi.C.UBX_LOGLEVEL_CRIT, nodename='running' }
   ib = ubx.block_get(nd, ubx.port_conns_totab(ubx.port_get(nd:b("ramp"), "out")).outgoing[1])
   sys:startup(nd)
end

function TestRunningChecks:teardown()
   ubx.node_rm(nd)
   nd = nil
end

function TestRunningChecks:TestStop()
   local ramp, trig = nd:b("ramp"), nd:b("trig")

   assert_equals(ubx.block_tostate(ramp, 'inactive'), EWRONG_STATE)
   -- a passive trigger stepped by a running one
   assert_equals(ubx.block_tostate(nd:b("inner"), 'inactive'), EWRONG_STATE)
   assert_equals(ubx.block_tostate(nd:b("sink"), 'inactive'), EWRONG_STATE)
   assert_equals(ubx.block_tostate(ib, 'inactive'), EWRONG_STATE)
   assert_equals(ubx.block_tostate(ib, 'preinit'), EWRONG_STATE)

   -- the thread owner, not the nearest trigger
   assert_equals(ubx.block_triggered_by(nd, nd:b("sink")), "trig")

   assert_equals(ubx.block_tostate(trig, 'inactive'), 0)
   -- inner is active, but no thread steps it anymore
   assert_equals(ubx.block_triggered_by(nd, nd:b("sink")), nil)
   assert_equals(ubx.block_tostate(nd:b("sink"), 'inactive'), 0)
   assert_equals(ubx.block_tostate(nd:b("inner"), 'inactive'), 0)
   assert_equals(ubx.block_tostate(ramp, 'inactive'), 0)
   assert_equals(ubx.block_tostate(ib, 'preinit'), 0)
end

-- passive triggers stepped by no thread, one pair in a chain cycle
local idle_sys = bd.system {
   imports = { "stdtypes", "trig", "ramp_uint64", "lfrb", "luablock" },
   blocks = {
      { name="ramp", type="ubx/ramp_uint64" },
      { name="sink", type="ubx/luablock" },
      { name="idle", type="ubx/trig" },
      { name="ca", type="ubx/trig" },
      { name="cb", type="ubx/trig" },
   },
   connections = {
      { src="ramp.out", tgt="sink.in" },
   },
   configurations = {
      { name="ramp", config = { start=0, slope=1 } },
      { name="sink", config = { lua_str=sink } },
      { name="idle", config = { chain0={ { b="#ramp" }, { b="#sink" } } } },
      { name="ca", config = { chain0={ { b="#cb" } } } },
      { name="cb", config = { chain0={ { b="#ca" }, { b="#sink" } } } },
   },
}

TestIdleTrigger = {}

function TestIdleTrigger:setup()
   nd = idle_sys:launch{ loglevel=ffi.C.UBX_LOGLEVEL_CRIT, nodename='idle' }
end

TestIdleTrigger.teardown = TestRunningChecks.teardown

function TestIdleTrigger:TestNotRunning()
   local ib = ubx.block_get(nd, ubx.port_conns_totab(ubx.port_get(nd:b("ramp"), "out")).outgoing[1])
   assert_equals(ubx.block_triggered_by(nd, nd:b("sink")), nil)
   assert_equals(ubx.block_triggered_by(nd, nd:b("ca")), nil)
   assert_true(ubx.connect(nd, "ramp", "out", "sink", "in2"))
   assert_equals(ubx.block_tostate(ib, 'inactive'), 0)
   assert_equals(ubx.block_tostate(nd:b("sink"), 'inactive'), 0)
   assert_equals(ubx.block_tostate(nd:b("ramp"), 'inactive'), 0)
   -- stepping refuses the stopped chain members
   ubx.cblock_step(nd:b("idle"))
end

function TestRunningChecks:TestLuablockThreadIsActive()
   local b = ubx.block_create(nd, "ubx/luablock", "lb", { lua_str=sink, thread=1, period=10 })
   assert_false(ubx.block_isactive(b))
   assert_equals(ubx.block_tostate(b, 'inactive'), 0)
   assert_true(ubx.block_isactive(b))
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
