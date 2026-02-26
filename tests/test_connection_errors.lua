local lu = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")
local ffi = require("ffi")

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_WARN
ubx.color = false

local ni

TestConnectionErrors = {}

function TestConnectionErrors:teardown()
   if ni then ubx.node_rm(ni) end
   ni = nil
   ubx.reset_block_uid()
end

--- Test that connecting ports with mismatched types fails validation
function TestConnectionErrors:TestTypeMismatch()
   local sys = bd.system {
      imports = { "stdtypes", "lfds_cyclic", "ramp_uint32", "ramp_int32" },
      blocks = {
	 { name = "r_uint", type = "ubx/ramp_uint32" },
	 { name = "r_int", type = "ubx/ramp_int32" },
      },
      configurations = {
	 { name = "r_uint", config = { start = 0, slope = 1 } },
	 { name = "r_int", config = { start = 0, slope = 1 } },
      },
      connections = {
	 { src = "r_uint.out", tgt = "r_int.out" },
      },
   }

   -- connecting two output ports should fail (tgt port is not an input)
   -- This will either fail validation or fail at launch
   local ok, err = pcall(function()
      ni = sys:launch({ nodename = "TestTypeMismatch", nostart = true, loglevel = LOGLEVEL })
   end)
   -- expect failure
   lu.assert_false(ok, "expected connection to fail for type/direction mismatch")
end

--- Test that connecting to a non-existent port fails
function TestConnectionErrors:TestNonExistentPort()
   local sys = bd.system {
      imports = { "stdtypes", "lfds_cyclic", "ramp_uint32" },
      blocks = {
	 { name = "r1", type = "ubx/ramp_uint32" },
	 { name = "r2", type = "ubx/ramp_uint32" },
      },
      configurations = {
	 { name = "r1", config = { start = 0, slope = 1 } },
	 { name = "r2", config = { start = 0, slope = 1 } },
      },
      connections = {
	 { src = "r1.out", tgt = "r2.nonexistent" },
      },
   }

   local ok, err = pcall(function()
      ni = sys:launch({ nodename = "TestNonExistentPort", nostart = true, loglevel = LOGLEVEL })
   end)
   lu.assert_false(ok, "expected connection to fail for non-existent port")
end

--- Test that connecting to a non-existent block fails validation
function TestConnectionErrors:TestNonExistentBlock()
   local sys = bd.system {
      imports = { "stdtypes", "lfds_cyclic", "ramp_uint32" },
      blocks = {
	 { name = "r1", type = "ubx/ramp_uint32" },
      },
      configurations = {
	 { name = "r1", config = { start = 0, slope = 1 } },
      },
      connections = {
	 { src = "r1.out", tgt = "ghost_block.in" },
      },
   }

   local numerr = sys:validate(false)
   lu.assert_true(numerr > 0, "expected validation errors for unknown block")
end

--- Test fan-out: one output connected to two inputs launches and connects
function TestConnectionErrors:TestFanOut()
   local sys = bd.system {
      imports = { "stdtypes", "lfds_cyclic", "saturation_double" },
      blocks = {
	 { name = "sat1", type = "ubx/saturation_double" },
	 { name = "sat2", type = "ubx/saturation_double" },
	 { name = "sat3", type = "ubx/saturation_double" },
      },
      configurations = {
	 { name = "sat1", config = { lower_limits = -10, upper_limits = 10 } },
	 { name = "sat2", config = { lower_limits = -10, upper_limits = 10 } },
	 { name = "sat3", config = { lower_limits = -10, upper_limits = 10 } },
      },
      connections = {
	 { src = "sat1.out", tgt = "sat2.in" },
	 { src = "sat1.out", tgt = "sat3.in" },
      },
   }

   local numerr = sys:validate(false)
   lu.assert_equals(numerr, 0)
   ni = sys:launch({ nodename = "TestFanOut", nostart = true, loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)

   -- verify connection table: sat1.out should have two outgoing connections
   local conntab = ubx.build_conntab(ni)
   lu.assert_equals(#conntab.sat1[2].out.outgoing, 2)
   lu.assert_equals(#conntab.sat2[1]['in'].incoming, 1)
   lu.assert_equals(#conntab.sat3[1]['in'].incoming, 1)
end

--- Test fan-in: two outputs connected to same input
function TestConnectionErrors:TestFanIn()
   local sys = bd.system {
      imports = { "stdtypes", "lfds_cyclic", "saturation_double" },
      blocks = {
	 { name = "sat1", type = "ubx/saturation_double" },
	 { name = "sat2", type = "ubx/saturation_double" },
	 { name = "sat3", type = "ubx/saturation_double" },
      },
      configurations = {
	 { name = "sat1", config = { lower_limits = -10, upper_limits = 10 } },
	 { name = "sat2", config = { lower_limits = -10, upper_limits = 10 } },
	 { name = "sat3", config = { lower_limits = -10, upper_limits = 10 } },
      },
      connections = {
	 { src = "sat1.out", tgt = "sat3.in" },
	 { src = "sat2.out", tgt = "sat3.in" },
      },
   }

   local numerr = sys:validate(false)
   lu.assert_equals(numerr, 0)
   ni = sys:launch({ nodename = "TestFanIn", nostart = true, loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)
end

--- Test that buffer_len config is applied to auto-created iblock
function TestConnectionErrors:TestBufferLenConfig()
   local sys = bd.system {
      imports = { "stdtypes", "lfds_cyclic", "saturation_double" },
      blocks = {
	 { name = "sat1", type = "ubx/saturation_double" },
	 { name = "sat2", type = "ubx/saturation_double" },
      },
      configurations = {
	 { name = "sat1", config = { lower_limits = -10, upper_limits = 10 } },
	 { name = "sat2", config = { lower_limits = -10, upper_limits = 10 } },
      },
      connections = {
	 { src = "sat1.out", tgt = "sat2.in", config = { buffer_len = 42 } },
      },
   }

   ni = sys:launch({ nodename = "TestBufferLen", loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)
   lu.assert_equals(ni:b("i_00000001"):c("buffer_len"):tolua(), 42)
end

os.exit(lu.LuaUnit.run())
