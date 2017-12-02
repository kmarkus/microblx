--
-- testsuite for testing ffi cdata to Lua conversion and vice versa
--

local lu=require"luaunit"

local ffi=require"ffi"
local ubx=require"ubx"
local utils=require"utils"
local cdata=require"cdata"

local assert_true = lu.assert_true
local assert_false = lu.assert_false
local assert_equals = lu.assert_equals
local assert_not_equals = lu.assert_not_equals

local ni=ubx.node_create("cdata_tolua_test")

ubx.load_module(ni, "stdtypes")
ubx.load_module(ni, "testtypes")
ubx.load_module(ni, "kdl_types")

function test_vector()
   local init = {x=1,y=2,z=3}
   local v1 = ffi.new("struct kdl_vector", init)

   local val=cdata.tolua(v1)
   assert_true(utils.table_cmp(val, init), "A: table->vector rountrip comparison error")
end

function test_vector_inv()
   local init = {x=1,y=2,z=3}
   local v1 = ffi.new("struct kdl_vector", init)
   v1.x=33
   v1.z=55
   local val=cdata.tolua(v1)
   assert_false(utils.table_cmp(val, init), "B: table->vector rountrip comparison error")
end

function test_frame()
   local init = {
      M={ data = {
	     1, 0, 0,
	     0, 1, 0,
	     0, 0, 1 },
      },

      p={ x=1.2, y=2.2, z=3.2 }
   }

   local f1 = ffi.new("struct kdl_frame", init)
   local val=cdata.tolua(f1)
   assert_true(utils.table_cmp(val, init), "C: table->frame rountrip comparison error")
end

function test_frame_inv()
   local init = {
      M={ data = {
	     1, 0, 0,
	     0, 1, 0,
	     0, 0, 1 },
      },
      p={ x=1.2, y=2.2, z=3.2 }
   }
   local f1 = ffi.new("struct kdl_frame", init)
   init.p.x=33
   local val=cdata.tolua(f1)
   assert_false(utils.table_cmp(val, init), "D: table->frame rountrip comparison error")
end

function test_char()
   local init = {
      name="Frodo Baggins",
      benchmark=993
   }

   local c = ffi.new("struct test_trig_conf", init)
   local val=cdata.tolua(c)
   assert_true(utils.table_cmp(val, init), "E: table->char rountrip comparison error")
end


function test_int()
   local init=33
   local i = ffi.new("unsigned int", init)
   local val=cdata.tolua(i)
   assert_equals(val, init, "F: table->int rountrip comparison error")
end

function test_int_inv()
   local init=33
   local i = ffi.new("unsigned int", init)
   local val=cdata.tolua(i)
   assert_not_equals(init, val-1, "G: table->int rountrip comparison error")
end

function test_ubx_data()
   local ubx_data_vect = ubx.data_alloc(ni, "struct kdl_vector", 1)
   local init = { x=7, y=8, z=9 }
   ubx.data_set(ubx_data_vect, init, true)
   local val = ubx.data_tolua(ubx_data_vect)
   assert_true(utils.table_cmp(val, init), "H: table->ubx_data(vector) rountrip comparison error")
end

function test_ubx_data_inv()
   local ubx_data_vect = ubx.data_alloc(ni, "struct kdl_vector", 1)
   local init = { x=2, y=5, z=22 }
   ubx.data_set(ubx_data_vect, init, true)
   init.x=344
   local val = ubx.data_tolua(ubx_data_vect)
   assert_false(utils.table_cmp(val, init), "I: table->ubx_data(vector) rountrip comparison error")
end

function test_ubx_data_basic()
   local ubx_data_int = ubx.data_alloc(ni, "unsigned int", 1)
   local init = 4711
   ubx.data_set(ubx_data_int, init, false)

   local val = ubx.data_tolua(ubx_data_int)
   assert_equals(init, val, "J: table->ubx_data(int) rountrip comparison error")
end

function test_pointer_to_prim()
   local init = 333
   local i = ffi.new("unsigned int[1]", init )
   local ip = ffi.new("unsigned int*", i)

   assert_equals(init, tonumber(i[0]), "K: init failed")
   assert_equals(tonumber(i[0]), tonumber(ip[0]), "L: mismatch between int and int*")

   local val = cdata.tolua(ip)

   assert_equals(init, val, "L: mismatch after converting from cdata")

end

function test_arr_data()
   local d = ubx.data_alloc(ni, "double", 5)
   local init = {1.1,2.2,3.3,4.4,5.5}
   ubx.data_set(d, init)
   local res = ubx.data_tolua(d)
   assert_true(utils.table_cmp(res, init), "test_arr_data double[5] roundtrip failed")
end

function test_void_pointer_to_prim()
   local init = 0xdeafbeaf
   local vp = ffi.new("void *", ffi.cast('void *', init))
   local val = cdata.tolua(vp)
   assert_equals(init, val, "L: mismatch after converting from cdata")
end

os.exit( lu.LuaUnit.run() )
