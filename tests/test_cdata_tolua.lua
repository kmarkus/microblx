
local ffi=require"ffi"
local lunit=require"lunit"
local u5c=require"u5c"
local utils=require"utils"
local cdata=require"cdata"

module("cdata_tolua_test", lunit.testcase, package.seeall)

ni=u5c.node_create("unit_test_node")
u5c.load_module(ni, "std_types/stdtypes/stdtypes.so")
u5c.load_module(ni, "std_types/testtypes/testtypes.so")
u5c.ffi_load_types(ni)

function test_vector()
   local init = {x=1,y=2,z=3}
   local v1 = ffi.new("struct Vector", init)

   local val=cdata.tolua(v1)
   assert_true(utils.table_cmp(val, init), "test_vector: table->vector rountrip comparison error")
end

function test_vector_inv()
   local init = {x=1,y=2,z=3}
   local v1 = ffi.new("struct Vector", init)
   v1.x=33
   v1.z=55
   local val=cdata.tolua(v1)
   assert_false(utils.table_cmp(val, init), "test_vector_inv: table->vector rountrip comparison error")
end

function test_frame()
   local init = {
      M={ X={ x=1, y=0, z=0 },
	  Y={ x=0, y=1, z=0 },
	  Z={ x=0, y=0, z=1 } },
      p={ x=1.2, y=2.2, z=3.2 } 
   }
   
   local f1 = ffi.new("struct Frame", init)
   local val=cdata.tolua(f1)
   assert_true(utils.table_cmp(val, init), "test_frame: table->frame rountrip comparison error")
end

function test_frame_inv()
   local init = {
      M={ X={ x=1, y=0, z=0 },
	  Y={ x=0, y=1, z=0 },
	  Z={ x=0, y=0, z=1 } },
      p={ x=1.2, y=2.2, z=3.2 } 
   }
   local f1 = ffi.new("struct Frame", init)
   init.M.Y.z=33
   local val=cdata.tolua(f1)
   assert_false(utils.table_cmp(val, init), "test_frame_inv: table->frame rountrip comparison error")
end

function test_char()
   local init = {
      name="Frodo Baggins",
      benchmark=993
   }
   
   local c = ffi.new("struct test_trig_conf", init)
   local val=cdata.tolua(c)
   assert_true(utils.table_cmp(val, init), "test_char: table->char rountrip comparison error")
end

   
function test_int()
   local init=33
   local i = ffi.new("unsigned int", init)
   local val=cdata.tolua(i)
   assert_equal(val, init, "test_int: table->int rountrip comparison error")
end

function test_int_inv()
   local init=33
   local i = ffi.new("unsigned int", init)
   local val=cdata.tolua(i)
   assert_not_equal(init, val-1, "test_int: table->int rountrip comparison error")
end

function test_u5c_data()
   local u5c_data_vect = u5c.data_alloc(ni, "testtypes/struct Vector", 1)
   local init = { x=7, y=8, z=9 }
   u5c.data_set(u5c_data_vect, init, true)
   local val = u5c.data_tolua(u5c_data_vect)
   assert_true(utils.table_cmp(val, init), "test_u5c_data: table->u5c_data(vector) rountrip comparison error")
end

function test_u5c_data_inv()
   local u5c_data_vect = u5c.data_alloc(ni, "testtypes/struct Vector", 1)
   local init = { x=2, y=5, z=22 }
   u5c.data_set(u5c_data_vect, init, true)
   init.x=344
   local val = u5c.data_tolua(u5c_data_vect)
   assert_false(utils.table_cmp(val, init), "test_u5c_data: table->u5c_data(vector) rountrip comparison error")
end

function test_u5c_data_basic()
   local u5c_data_int = u5c.data_alloc(ni, "unsigned int", 1)
   local init = 4711
   u5c.data_set(u5c_data_int, init, true)
   local val = u5c.data_tolua(u5c_data_int)
   assert_true(utils.table_cmp(val, init), "test_u5c_data: table->u5c_data(int) rountrip comparison error")
end

function test_pointer_to_prim()
   local init = 333
   local i = ffi.new("unsigned int[1]", init )
   local ip = ffi.new("unsigned int*", i)
   
   assert_equal(init, tonumber(i[0]), "pointer_to_prim: init failed")
   assert_equal(tonumber(i[0]), tonumber(ip[0]), "pointer_to_prim: mismatch between int and int*")

   local val = cdata.tolua(ip)

   assert_equal(init, val, "pointer_to_prim: mismatch after converting from cdata")

end