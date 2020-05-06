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
      {	name="rnd1", config = {
	   loglevel=6,
	   min_max_config = { min=1, max=2 } }
      },
      {	name="rnd2", config = {
	   loglevel=4,
	   min_max_config = { min=3, max=4 } }
      },
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
      {	name="leaf/rnd2", config = {
	   loglevel=5,
	   min_max_config = { min=14, max=15 } }
      },
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
      {	name="mid/leaf/rnd2", config = {
	   loglevel=7,
	   min_max_config = { min=106, max=107 } }
      },
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
   lu.assert_equals( NI:b("rnd1"):c("loglevel"):tolua(), 6)
   lu.assert_equals( NI:b("rnd2"):c("loglevel"):tolua(), 4)

   -- check connections
   local conntab_act = ubx.build_conntab(NI)
   local conntab_exp = {
      rnd1={
	 {seed={incoming={}, outgoing={}}},
	 {rnd={incoming={}, outgoing={"i_0000000d"}}}
      },
      rnd2={
	 {seed={incoming={"i_0000000d"}, outgoing={}}},
	 {rnd={incoming={}, outgoing={}}}
      },
      trig={{tstats={incoming={}, outgoing={}}}}
   }
   lu.assert_equals(conntab_act, conntab_exp)
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

   -- test overwriting of individual configs
   lu.assert_equals( NI:b("leaf/rnd1"):c("loglevel"):tolua(), 6 )
   lu.assert_equals( NI:b("leaf/rnd2"):c("loglevel"):tolua(), 5 )

   -- check connections
   local conntab_act = ubx.build_conntab(NI)
   local conntab_exp = {
      ["leaf/rnd1"] = {
	 {seed={incoming={"i_00000002"}, outgoing={}}},
	 {rnd={incoming={}, outgoing={"i_00000004"}}}
      },
      ["leaf/rnd2"] = {
	 {seed={incoming={"i_00000004"}, outgoing={}}},
	 {rnd={incoming={}, outgoing={"i_00000003"}}}
      },
      ["leaf/trig"] = {{tstats={incoming={}, outgoing={}}}},
      rnd1={
	 {seed={incoming={"i_00000003"}, outgoing={}}},
	 {rnd={incoming={}, outgoing={"i_00000001"}}}
      },
      rnd2={
	 {seed={incoming={"i_00000001"}, outgoing={}}},
	 {rnd={incoming={}, outgoing={"i_00000002"}}}
      },
      trig={{tstats={incoming={}, outgoing={}}}}
   }

   lu.assert_equals(conntab_act, conntab_exp)
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

   -- test overriding of individual configs
   lu.assert_equals( NI:b("mid/leaf/rnd1"):c("loglevel"):tolua(), 6 )
   lu.assert_equals( NI:b("mid/leaf/rnd2"):c("loglevel"):tolua(), 7 )
end

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


function TestComp:test_late_config()
   local type_name = 'struct kdl_vector'
   local data_len = 1
   local value = { x=11.1, y=222.2, z=333.3 }

   NI = bd.system {
      imports = { 'stdtypes', 'cconst', 'testtypes', 'lfds_cyclic' },
      blocks = { { name="cconst1", type="consts/cconst" } },
      configurations = {
	 { name="cconst1",
	   config = {
	      type_name=type_name, data_len=data_len, value=value
	   }
	 }
      },
   }:launch{nodename = 'test_late_config' }

   lu.assert_not_nil(NI)
   local cconst1 = NI:b('cconst1')
   lu.assert_not_nil(cconst1)
   local port = ubx.port_clone_conn(cconst1, 'out')
   lu.assert_not_nil(port)

   for _=1,10 do
      cconst1:do_step()
      local len, val = port:read()
      lu.assert_equals(tonumber(len), data_len)
      lu.assert_equals(val:tolua(), value)
   end
end

function TestComp:test_late_config2()
   local type_name = 'struct kdl_vector'
   local data_len = 3
   local value = {
      { x=1, y=2, z=3 },
      { x=4, y=5, z=6 },
      { x=7, y=8, z=9 }
   }

   NI = bd.system {
      imports = { 'stdtypes', 'cconst', 'testtypes', 'lfds_cyclic' },
      blocks = { { name="cconst1", type="consts/cconst" } },
      configurations = {
	 { name="cconst1",
	   config = {
	      type_name=type_name, data_len=data_len, value=value
	   }
	 }
      },
   }:launch{nodename = 'test_late_config2' }

   lu.assert_not_nil(NI)
   local cconst1 = NI:b('cconst1')
   lu.assert_not_nil(cconst1)
   local port = ubx.port_clone_conn(cconst1, 'out')
   lu.assert_not_nil(port)

   for _=1,10 do
      cconst1:do_step()
      local len, val = port:read()
      lu.assert_equals(tonumber(len), data_len)
      lu.assert_equals(val:tolua(), value)
   end
end

os.exit( lu.LuaUnit.run() )
