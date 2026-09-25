--
-- Test triggee 'every' and 'num_steps' handling of the trig block
--

local lu = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")
local ffi = require("ffi")

local assert_equals = lu.assert_equals
local assert_not_nil = lu.assert_not_nil

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_WARN

local nd

TestTrigChain = {}

function TestTrigChain:teardown()
   if nd then ubx.node_rm(nd) end
   nd = nil
end

local sys = bd.system {
   imports = { "stdtypes", "trig", "ramp_uint64" },
   blocks = {
      { name = "r1", type = "ubx/ramp_uint64" },
      { name = "r2", type = "ubx/ramp_uint64" },
      { name = "trig", type = "ubx/trig" },
   },
   configurations = {
      { name = "r1", config = { start = 0, slope = 1 } },
      { name = "r2", config = { start = 0, slope = 1 } },
      { name = "trig", config = {
	   chain0 = {
	      { b = "#r1" },
	      { b = "#r2", every = 2, num_steps = 3 },
	   } } },
   },
}

--- 'every' skips steps, 'num_steps' multiplies them
function TestTrigChain:TestEveryNumSteps()
   nd = sys:launch{ nodename = "TestTrigChain", loglevel = LOGLEVEL }
   assert_not_nil(nd)

   local trig = nd:b("trig")
   local NUM_TRIGS = 4

   for _ = 1, NUM_TRIGS do
      ubx.cblock_step(trig)
   end

   -- r1: stepped on every trigger
   assert_equals(tonumber(nd:b("r1").stat_num_steps), NUM_TRIGS)

   -- r2: every=2 -> triggered on trig steps 1 and 3, num_steps=3
   -- -> 2 * 3 = 6 steps
   assert_equals(tonumber(nd:b("r2").stat_num_steps), 6)
end

--- block_unload refuses a block an active trigger still steps
function TestTrigChain:TestUnloadActiveTriggee()
   nd = sys:launch{ nodename = "TestUnloadActive", loglevel = LOGLEVEL }

   lu.assert_error_msg_contains("is triggered by active block 'trig'",
				ubx.block_unload, nd, "r2")
   assert_not_nil(ubx.ubx.ubx_block_get(nd, "r2"))
end

--- block_unload removes the block from inactive trigger chains
function TestTrigChain:TestUnloadScrubsChain()
   nd = sys:launch{ nodename = "TestUnloadScrub", loglevel = LOGLEVEL }

   local trig = nd:b("trig")
   ubx.block_tostate(trig, 'inactive')
   ubx.block_unload(nd, "r2")

   local chain = ubx.data_tolua(trig:c("chain0").value)
   assert_equals(chain.b, "r1")

   assert_equals(ubx.block_tostate(trig, 'active'), 0)
   ubx.cblock_step(trig)
   assert_equals(tonumber(nd:b("r1").stat_num_steps), 1)
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
