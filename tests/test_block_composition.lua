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
      {	name="rnd1", config = { min_max_config = { min=1, max=2 } } },
      {	name="rnd2", config = { min_max_config = { min=3, max=4 } } },
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
      {	name="rnd1", config = { min_max_config = { min=10, max=11 } } },
      {	name="rnd2", config = { min_max_config = { min=12, max=13 } } },
      {	name="leaf/rnd2", config = { min_max_config = { min=14, max=15 } } },
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

local comp2 = bd.system {
   imports = { "stdtypes", "random", "lfds_cyclic", "trig" },

   subsystems = {
      mid = utils.deepcopy(comp1),
   },
   blocks = {
      { name = "rnd1", type="random/random" },
      { name = "rnd2", type="random/random" },
      { name = "trig", type="std_triggers/trig" },
   },
   configurations = {
      {	name="rnd1", config = { min_max_config = { min=100, max=101 } } },
      {	name="rnd2", config = { min_max_config = { min=102, max=103 } } },
      {	name="mid/rnd2", config = { min_max_config = { min=104, max=105 } } },
      {	name="mid/leaf/rnd2", config = { min_max_config = { min=106, max=107 } } },
      { name="trig", config = { trig_blocks={
				   { b="#rnd1", num_steps=1, measure=0 },
				   { b="#rnd2", num_steps=1, measure=0 },
				   { b="#mid/trig", num_steps=1, measure=0 }} } }
   },
   connections = {
      { src="rnd1.rnd", tgt="rnd2.seed" },
      { src="rnd2.rnd", tgt="mid/rnd1.seed" },
      { src="mid/rnd2.rnd", tgt="rnd1.seed" },
      { src="mid/leaf/rnd2.rnd", tgt="rnd1.seed" },
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

   -- check configs
   lu.assert_equals( NI:b("rnd1"):c("min_max_config"):tolua(), { min=1, max=2 } )
   lu.assert_equals( NI:b("rnd2"):c("min_max_config"):tolua(), { min=3, max=4 } )

end

function TestComp:test_comp1()
   local num_err, res = comp1:validate(UMF_CHECK_VERBOSE)
   lu.assert_equals(num_err, 0)
   NI =	comp1:launch({ nodename="test_comp1", nostart=true})
   lu.assert_not_nil(NI)

   -- check configs
   lu.assert_equals( NI:b("rnd1"):c("min_max_config"):tolua(), { min=10, max=11 } )
   lu.assert_equals( NI:b("rnd2"):c("min_max_config"):tolua(), { min=12, max=13 } )
   lu.assert_equals( NI:b("leaf/rnd1"):c("min_max_config"):tolua(), { min=1, max=2 } )
   lu.assert_equals( NI:b("leaf/rnd2"):c("min_max_config"):tolua(), { min=14, max=15 } )
end

function TestComp:test_comp2()
   local num_err, res = comp2:validate(UMF_CHECK_VERBOSE)
   lu.assert_equals(num_err, 0)
   NI =	comp2:launch({ nodename="test_comp2", nostart=true})
   lu.assert_not_nil(NI)

   lu.assert_equals( NI:b("rnd1"):c("min_max_config"):tolua(), { min=100, max=101 } )
   lu.assert_equals( NI:b("rnd2"):c("min_max_config"):tolua(), { min=102, max=103 } )
   lu.assert_equals( NI:b("mid/rnd1"):c("min_max_config"):tolua(), { min=10, max=11 } )
   lu.assert_equals( NI:b("mid/rnd2"):c("min_max_config"):tolua(), { min=104, max=105 } )
   lu.assert_equals( NI:b("mid/leaf/rnd1"):c("min_max_config"):tolua(), { min=1, max=2 } )
   lu.assert_equals( NI:b("mid/leaf/rnd2"):c("min_max_config"):tolua(), { min=106, max=107 } )
end

local leaf = bd.system {
   imports = { "stdtypes", "random", "lfds_cyclic", "trig" },
   blocks = {
      { name = "rnd1", type="random/random" },
      { name = "rnd2", type="random/random" },
      { name = "trig", type="std_triggers/trig" },
   },
   configurations = {
      {	name="rnd1", config = { min_max_config = { min=1, max=2 } } },
      {	name="rnd2", config = { min_max_config = { min=3, max=4 } } },
      { name="trig", config = { trig_blocks={
				   { b="#rnd1", num_steps=1, measure=0 },
				   { b="#rnd2", num_steps=1, measure=0 } } } }
   },
   connections = {
      { src="rnd1.rnd", tgt="rnd2.seed" }
   },
}

--
-- Node config tests
--

local ndcfg1 = bd.system {
   imports = { "stdtypes", "random" },

   node_configurations = {
      rnd_conf = { type="struct random_config", config= { min=1000, max=2000 } }
   },

   blocks = {
      { name = "rnd1", type="random/random" },
      { name = "rnd2", type="random/random" },
      { name = "rnd3", type="random/random" },
   },
   configurations = {
      {	name="rnd1", config = { min_max_config = "&rnd_conf" } },
      {	name="rnd2", config = { min_max_config = "&rnd_conf" } },
      {	name="rnd3", config = { min_max_config = { min=555, max=777 } } },
   },
}

function TestComp:test_nodecfg1()
   local num_err, res = ndcfg1:validate(UMF_CHECK_VERBOSE)
   lu.assert_equals(num_err, 0)
   NI =	ndcfg1:launch({ nodename="test_nodecfg1", nostart=true})
   lu.assert_not_nil(NI)

      -- check configs
   lu.assert_equals( NI:b("rnd1"):c("min_max_config"):tolua(), { min=1000, max=2000 } )
   lu.assert_equals( NI:b("rnd2"):c("min_max_config"):tolua(), { min=1000, max=2000 } )
   lu.assert_equals( NI:b("rnd3"):c("min_max_config"):tolua(), { min=555, max=777 } )
end


local ndcfg2 = bd.system {
   imports = { "stdtypes", "random" },

   node_configurations = {
      rnd_conf = { type="struct random_config", config = { min=999, max=1111 } }
   },

   subsystems = { sub1 = utils.deepcopy(ndcfg1)  },

   blocks = {
      { name = "rnd1", type="random/random" },
      { name = "rnd2", type="random/random" },
   },

   configurations = {
      {	name="rnd1", config = { min_max_config = "&rnd_conf" } },
      {	name="rnd2", config = { min_max_config = "&rnd_conf" } },
   },
}

function TestComp:test_nodecfg2()
   local num_err, res = ndcfg2:validate(UMF_CHECK_VERBOSE)
   lu.assert_equals(num_err, 0)
   NI =	ndcfg2:launch({ nodename="test_nodecfg2", nostart=true})
   lu.assert_not_nil(NI)

   -- check configs
   lu.assert_equals( NI:b("rnd1"):c("min_max_config"):tolua(), { min=999, max=1111 } )
   lu.assert_equals( NI:b("rnd2"):c("min_max_config"):tolua(), { min=999, max=1111 } )
   lu.assert_equals( NI:b("sub1/rnd1"):c("min_max_config"):tolua(), { min=999, max=1111 } )
   lu.assert_equals( NI:b("sub1/rnd2"):c("min_max_config"):tolua(), { min=999, max=1111 } )
   lu.assert_equals( NI:b("sub1/rnd3"):c("min_max_config"):tolua(), { min=555, max=777 } )

end

local ndcfg3 = bd.system {
   imports = { "stdtypes", "random" },

   node_configurations = {
      rnd_conf = { type="struct random_config", config = { min=4444, max=8888 } }
   },

   subsystems = { sub2 = utils.deepcopy(ndcfg2)  },

   blocks = {
      { name = "rnd1", type="random/random" },
      { name = "rnd2", type="random/random" },
   },

   configurations = {
      {	name="rnd1", config = { min_max_config = "&rnd_conf" } },
      {	name="rnd2", config = { min_max_config = "&rnd_conf" } },
   },
}

function TestComp:test_nodecfg3()
   local num_err, res = ndcfg3:validate(UMF_CHECK_VERBOSE)
   lu.assert_equals(num_err, 0)
   NI =	ndcfg3:launch({ nodename="test_nodecfg3", nostart=true})
   lu.assert_not_nil(NI)

   -- check configs
   lu.assert_equals( NI:b("rnd1"):c("min_max_config"):tolua(), { min=4444, max=8888 } )
   lu.assert_equals( NI:b("rnd2"):c("min_max_config"):tolua(), { min=4444, max=8888 } )
   lu.assert_equals( NI:b("sub2/rnd1"):c("min_max_config"):tolua(), { min=4444, max=8888 } )
   lu.assert_equals( NI:b("sub2/rnd2"):c("min_max_config"):tolua(), { min=4444, max=8888 } )
   lu.assert_equals( NI:b("sub2/sub1/rnd1"):c("min_max_config"):tolua(), { min=4444, max=8888 } )
   lu.assert_equals( NI:b("sub2/sub1/rnd2"):c("min_max_config"):tolua(), { min=4444, max=8888 } )
   lu.assert_equals( NI:b("sub2/sub1/rnd3"):c("min_max_config"):tolua(), { min=555, max=777 } )
end

-- TODO
--  - connection testing (add is_connected?)

os.exit( lu.LuaUnit.run() )
