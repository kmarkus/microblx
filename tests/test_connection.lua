local lu=require("luaunit")
local u=require("utils")
local ubx=require("ubx")
local bd = require("blockdiagram")
local ffi = require("ffi")

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_INFO
local CHECK_VERBOSE = false

local ni
TestConnection = {}

function TestConnection:setup()
   ubx.reset_block_uid()
end

function TestConnection:teardown()
   if ni then ubx.node_rm(ni) end
   ni = nil
end

function TestConnection:Test_01_Simple()
   local DATA_LEN = 1
   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "saturation" },
      blocks = {
	 { name = "sat1", type = "ubx/saturation" },
	 { name = "sat2", type = "ubx/saturation" }
      },
      configurations = {
	 { name = "sat1", config = {
	      type="double",
	      data_len=DATA_LEN,
	      lower_limits = u.fill(-10, DATA_LEN),
	      upper_limits = u.fill(3.4, DATA_LEN), } },
	 { name = "sat2", config = {
	      type="double",
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
      imports = { "stdtypes", "mqueue", "saturation" },
      blocks = {
	 { name = "sat1", type = "ubx/saturation" },
	 { name = "mq1", type = "ubx/mqueue" },
      },
      configurations = {
	 { name = "sat1", config = {
	      type="double",
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
      imports = { "stdtypes", "mqueue", "saturation" },
      blocks = {
	 { name = "sat1", type = "ubx/saturation" },
      },
      configurations = {
	 { name = "sat1", config = {
	      type="double",
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
      imports = { "stdtypes", "mqueue", "saturation" },
      blocks = {
	 { name = "sat1", type = "ubx/saturation" },
      },
      configurations = {
	 { name = "sat1", config = {
	      type="double",
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

--- a config table shared by connections of different type and length
--- must not be modified by connect
function TestConnection:Test_05_SharedConfig()
   local cfg = { buffer_len = 16 }
   local function sat(typ, len)
      return { type=typ, data_len=len, lower_limits=-1, upper_limits=1 }
   end

   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "saturation" },
      blocks = {
	 { name = "sat1", type = "ubx/saturation" },
	 { name = "sat2", type = "ubx/saturation" },
	 { name = "sat3", type = "ubx/saturation" },
	 { name = "sat4", type = "ubx/saturation" },
      },
      configurations = {
	 { name = "sat1", config = sat("double", 1) },
	 { name = "sat2", config = sat("double", 1) },
	 { name = "sat3", config = sat("int32_t", 3) },
	 { name = "sat4", config = sat("int32_t", 3) },
      },
      connections = {
	 { src="sat1.out", tgt="sat2.in", config=cfg },
	 { src="sat3.out", tgt="sat4.in", config=cfg },
      },
   }

   ni = sys:launch({nodename = "TestSharedConfig", loglevel=LOGLEVEL })
   lu.assert_not_nil(ni)

   lu.assert_equals(cfg, { buffer_len = 16 })

   local i1, i2 = ni:b("i_00000001"), ni:b("i_00000002")
   lu.assert_equals(i1:c("type_name"):tolua(), "double")
   lu.assert_equals(i1:c("data_len"):tolua(), 1)
   lu.assert_equals(i2:c("type_name"):tolua(), "int32_t")
   lu.assert_equals(i2:c("data_len"):tolua(), 3)
   lu.assert_equals(i2:c("buffer_len"):tolua(), 16)
end

--- removing a connected iblock must drop the port references to it
function TestConnection:Test_06_IBlockRmDisconnects()
   ni = ubx.node_create("Test_06", { loglevel = LOGLEVEL })
   for _,m in ipairs{ "stdtypes", "lfrb", "ramp_double", "math_double" } do
      ubx.load_module(ni, m)
   end
   local r = ubx.block_create(ni, "ubx/ramp_double", "r", { start=0, slope=1 })
   local m = ubx.block_create(ni, "ubx/math_double", "m", { func="sin" })
   ubx.block_init(r)
   ubx.block_init(m)
   assert(ubx.connect(ni, "r", "out", "m", "x", "ubx/lfrb", {}))

   local ib = ubx.port_totab(r:p("out")).connections.outgoing[1]
   lu.assert_not_nil(ib)
   ubx.block_unload(ni, ib)

   lu.assert_equals(ubx.port_totab(r:p("out")).connections.outgoing, {})
   lu.assert_equals(ubx.port_totab(m:p("x")).connections.incoming, {})
end

if not _RUNNER then os.exit( lu.LuaUnit.run() ) end
