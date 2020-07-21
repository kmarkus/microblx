local lu=require("luaunit")
local u=require("utils")
local ubx=require("ubx")
local bd = require("blockdiagram")
local ffi = require("ffi")

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_INFO
local CHECK_VERBOSE = false
ubx.color=false

local ni
TestConnection = {}

function TestConnection:teardown()
   if ni then ubx.node_rm(ni) end
   ni = nil
   ubx.reset_block_uid()
end

function TestConnection:Test_01_Simple()
   local DATA_LEN = 1
   local sys = bd.system {
      imports = { "stdtypes", "lfds_cyclic", "saturation_double" },
      blocks = {
	 { name = "sat1", type = "ubx/saturation_double" },
	 { name = "sat2", type = "ubx/saturation_double" }
      },
      configurations = {
	 { name = "sat1", config = {
	      data_len=DATA_LEN,
	      lower_limits = u.fill(-10, DATA_LEN),
	      upper_limits = u.fill(3.4, DATA_LEN), } },
	 { name = "sat2", config = {
	      data_len=DATA_LEN,
	      lower_limits = u.fill(-2, DATA_LEN),
	      upper_limits = u.fill(2, DATA_LEN), } }
      },
      connections = {
	 { src="sat1.out", tgt="sat2.in", config={ buffer_len = 100 } }
      },
   }
   local num_err = sys:validate(CHECK_VERBOSE)
   lu.assert_equals(num_err, 0)
   ni = sys:launch({nodename = "TestLen1", loglevel=LOGLEVEL })
   lu.assert_not_nil(ni);
   lu.assert_equals( ni:b("i_00000001"):c("buffer_len"):tolua(), 100 )

   local conntab_act = ubx.build_conntab(ni)
   local conntab_exp = {
      sat1={{['in']={incoming={}, outgoing={}}}, {out={incoming={}, outgoing={"i_00000001"}}}},
      sat2={{['in']={incoming={"i_00000001"}, outgoing={}}}, {out={incoming={}, outgoing={}}}}
   }

   lu.assert_equals(conntab_act, conntab_exp)
end

function TestConnection:Test_02_MQExisting()
   local DATA_LEN = 10
   local sys = bd.system {
      imports = { "stdtypes", "mqueue", "saturation_double" },
      blocks = {
	 { name = "sat1", type = "ubx/saturation_double" },
	 { name = "mq1", type = "ubx/mqueue" },
      },
      configurations = {
	 { name = "sat1", config = {
	      data_len=DATA_LEN,
	      lower_limits = u.fill(-10, DATA_LEN),
	      upper_limits = u.fill(3.4, DATA_LEN), } },
	 { name = "mq1", config = {
	      mq_id = "mymq1", type_name = "double", data_len = DATA_LEN, buffer_len = 4 }
	 }
      },
      connections = {
	 { src="sat1.out", tgt="mq1", config={ buffer_len = 100 } }
      },
   }
   local num_err = sys:validate(CHECK_VERBOSE)
   lu.assert_equals(num_err, 0)
   ni = sys:launch({nodename = "MQExisting", loglevel=LOGLEVEL })
   lu.assert_not_nil(ni);

   -- the buffer_len=100 should have been ignored with a warning
   lu.assert_equals( ni:b("mq1"):c("buffer_len"):tolua(), 4)

   local conntab_act = ubx.build_conntab(ni)
   local conntab_exp = {
      sat1={{['in']={incoming={}, outgoing={}}}, {out={incoming={}, outgoing={"mq1"}}}},
   }

   lu.assert_equals(conntab_act, conntab_exp)
end

function TestConnection:Test_03_MQNonExisting()
   local DATA_LEN = 10
   local sys = bd.system {
      imports = { "stdtypes", "mqueue", "saturation_double" },
      blocks = {
	 { name = "sat1", type = "ubx/saturation_double" },
      },
      configurations = {
	 { name = "sat1", config = {
	      data_len=DATA_LEN,
	      lower_limits = u.fill(-10, DATA_LEN),
	      upper_limits = u.fill(3.4, DATA_LEN), } },
      },
      connections = {
	 { src="sat1.out", type="ubx/mqueue", config={ buffer_len = 2 } }
      },
   }
   local num_err = sys:validate(CHECK_VERBOSE)
   lu.assert_equals(num_err, 0)
   ni = sys:launch({nodename = "MQNonExisting", loglevel=LOGLEVEL })
   lu.assert_not_nil(ni);
   lu.assert_equals( ni:b("i_00000001"):c("buffer_len"):tolua(), 2 )

   local conntab_act = ubx.build_conntab(ni)
   local conntab_exp = {
      sat1={{['in']={incoming={}, outgoing={}}}, {out={incoming={}, outgoing={"i_00000001"}}}},
   }

   lu.assert_equals(conntab_act, conntab_exp)
end

function TestConnection:Test_04_MQNonExisting()
   local DATA_LEN = 10
   local sys = bd.system {
      imports = { "stdtypes", "mqueue", "saturation_double" },
      blocks = {
	 { name = "sat1", type = "ubx/saturation_double" },
      },
      configurations = {
	 { name = "sat1", config = {
	      data_len=DATA_LEN,
	      lower_limits = u.fill(-10, DATA_LEN),
	      upper_limits = u.fill(3.4, DATA_LEN), } },
      },
      connections = {
	 { tgt="sat1.in", type="ubx/mqueue", config={ buffer_len = 2 } }
      },
   }
   local num_err = sys:validate(CHECK_VERBOSE)
   lu.assert_equals(num_err, 0)
   ni = sys:launch({nodename = "MQNonExisting2", loglevel=LOGLEVEL })
   lu.assert_not_nil(ni);
   lu.assert_equals( ni:b("i_00000001"):c("buffer_len"):tolua(), 2 )

   local conntab_act = ubx.build_conntab(ni)
   local conntab_exp = {
      sat1={
	 {['in']={incoming={"i_00000001"}, outgoing={}}}, {out={incoming={}, outgoing={}}}},
   }

   lu.assert_equals(conntab_act, conntab_exp)
end

os.exit( lu.LuaUnit.run() )
