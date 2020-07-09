local lu = require("luaunit")
local ubx = require("ubx")
local utils = require("utils")
local bd = require("blockdiagram")
local time = require("time")
local ffi = require("ffi")

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_DEBUG
local CHECK_VERBOSE = false

local ni

TestSaturation = {}

function TestSaturation:teardown()
   if ni then ubx.node_rm(ni) end
   ni = nil
end

function TestSaturation:TestLen1()

   local sys = bd.system {
      imports = { "stdtypes", "lfds_cyclic", "saturation_double" },
      blocks = { { name = "satd1", type = "ubx/saturation_double" } },
      configurations = {
	 { name = "satd1", config = { lower_limits = -10, upper_limits = 3.3 } }
      }
   }

   -- testdata
   local in_data =  { 1, 0, 2, 5,  10000, 3.2, -1, -1000, -9.9999, -10.1 }
   local exp_data = { 1, 0, 2, 3.3, 3.3,  3.2, -1, -10,   -9.9999, -10 }
   assert(#in_data == #exp_data)

   local num_err = sys:validate(CHECK_VERBOSE)
   lu.assert_equals(num_err, 0)
   ni = sys:launch({nodename = "TestLen1", loglevel=LOGLEVEL })
   lu.assert_not_nil(ni);

   local satd1 = ni:b("satd1")
   local pin = ubx.port_clone_conn(satd1, "in", 1, nil, 7, 0)
   local pout = ubx.port_clone_conn(satd1, "out", nil, 1, 7, 0)


   for i=1,#in_data do
      pin:write(in_data[i])
      satd1:do_step()
      local len, val = pout:read()
      lu.assert_equals(tonumber(len), 1)
      lu.assert_equals(val:tolua(), exp_data[i])
   end
end

function TestSaturation:TestLen5()
   local data_len = 5

   local sys = bd.system {
      imports = { "stdtypes", "lfds_cyclic", "saturation_double" },
      blocks = { { name = "satd1", type = "ubx/saturation_double" } },
      configurations = {
	 { name = "satd1", config = {
	      data_len = data_len,
	      lower_limits = { -1, -2, -3, -4, -5 },
	      upper_limits = { 1, 22, 333, 4444, 55555 } }
	 }
      }
   }

   local in_data =  {
      {	0, 0, 0, 0, 0 },
      { 5, 5, 5, 5, 5 },
      { 1, 400, 400, 400, 400 },
      { 22, 333, 4444, 55555, 666666 },
      { 55555, 4444, 333, 22, 1 },

      { -1.1, -2.2, -3.3, -4.4, -5.5 },
      { 0, -10, 0, -10, 0 },
      { -666666, -666666, -666666, -666666, -6666660 },

   }

   local exp_data = {
      {	0, 0, 0, 0, 0 },
      { 1, 5, 5, 5, 5 },
      { 1, 22, 333, 400, 400 },
      { 1, 22, 333, 4444, 55555 },
      { 1, 22, 333, 22, 1 },

      { -1, -2, -3, -4, -5 },
      { 0, -2, 0, -4, 0 },
      { -1, -2, -3, -4, -5 }
   }

   assert(#in_data == #exp_data)
   for i=1,#in_data do
      assert(#in_data[i] == data_len);
      assert(#exp_data[i] == data_len);
   end

   local num_err = sys:validate(CHECK_VERBOSE)
   lu.assert_equals(num_err, 0)
   ni = sys:launch({nodename = "TestLen5", loglevel=LOGLEVEL })
   lu.assert_not_nil(ni);

   local satd1 = ni:b("satd1")
   local pin = ubx.port_clone_conn(satd1, "in", 1, nil, 7, 0)
   local pout = ubx.port_clone_conn(satd1, "out", nil, 1, 7, 0)

   for i=1,#in_data do
      pin:write(in_data[i])
      satd1:do_step()
      local len, val = pout:read()
      lu.assert_equals(tonumber(len), data_len)
      lu.assert_equals(val:tolua(), exp_data[i])
   end
end

os.exit( lu.LuaUnit.run() )
