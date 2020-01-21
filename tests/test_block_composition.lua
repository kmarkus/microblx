local lu = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")
local utils = require("utils")

local UMF_CHECK_VERBOSE=false

ubx.color=false

---
--- simple composition
---

local leaf = bd.system {
   imports = { "stdtypes", "random", "lfds_cyclic", "trig" },
   blocks = {
      { name = "rnd1", type="random/random" },
      { name = "rnd2", type="random/random" },
      { name = "trig", type="std_triggers/trig" },
   },
   configurations = {
      {	name="rnd1", config = { min_max_config = { min=11, max=22 } } },
      {	name="rnd2", config = { min_max_config = { min=111, max=222 } } },
      { name="trig", config = { trig_blocks={
				   { b="#rnd1", num_steps=1, measure=0 },
				   { b="#rnd2", num_steps=1, measure=0 } } } }
   },
   connections = {
      { src="rnd1.rnd", tgt="rnd2.seed" }
   },
}

local comp1 = bd.system {
   imports = { "stdtypes", "random", "lfds_cyclic", "trig" },

   subsystems = {
      leaf = utils.deepcopy(leaf),
   },
   blocks = {
      { name = "rnd1", type="random/random" },
      { name = "rnd2", type="random/random" },
      { name = "trig", type="std_triggers/trig" },
   },
   configurations = {
      {	name="rnd1", config = { min_max_config = { min=33, max=44 } } },
      {	name="rnd2", config = { min_max_config = { min=444, max=555 } } },
      {	name="leaf/rnd2", config = { min_max_config = { min=89, max=99 } } },
      { name="trig", config = { trig_blocks={
				   { b="#rnd1", num_steps=1, measure=0 },
				   { b="#rnd2", num_steps=1, measure=0 },
				   { b="#leaf/trig", num_steps=1, measure=0 }} } }
   },
   connections = {
      { src="rnd1.rnd", tgt="rnd2.seed" },
      { src="rnd2.rnd", tgt="leaf/rnd1.seed" },
      { src="leaf/rnd2.rnd", tgt="rnd1.seed" },
   },
}

local NI

TestComp = {}

--function CompTest:setup() end

function TestComp:teardown()
   if NI then ubx.node_cleanup(NI) end
   NI = nil
end

function TestComp:test_leaf()
   local num_err, res = leaf:validate(UMF_CHECK_VERBOSE)
   lu.assert_equals(num_err, 0)
   NI =	leaf:launch({ nodename="test_leaf", nostart=true})
   lu.assert_not_nil(NI)
end

function TestComp:test_comp1()
   local num_err, res = comp1:validate(UMF_CHECK_VERBOSE)
   lu.assert_equals(num_err, 0)
   NI =	comp1:launch({ nodename="test_comp1", nostart=true})
   lu.assert_not_nil(NI)

   -- check configs
   lu.assert_equals(
      NI:b("rnd1"):c("min_max_config"):tolua(), { min=33, max=44 })

   lu.assert_equals(
      NI:b("rnd2"):c("min_max_config"):tolua(), { min=444, max=555 })

   -- currently fails due non-hierarchical config application
   lu.assert_equals(
      NI:b("leaf/rnd1"):c("min_max_config"):tolua(), { min=11, max=22 })

   lu.assert_equals(
      NI:b("leaf/rnd2"):c("min_max_config"):tolua(), { min=89, max=99 })

end

-- TODO
--  - node configs
--  - connection testing (add is_connected?)

os.exit( lu.LuaUnit.run() )
