local lu = require"luaunit"
local ffi=require"ffi"
local ubx=require"ubx"

local assert_equals = lu.assert_equals

local nd=ubx.node_create("data_init_test")

ubx.load_module(nd, "stdtypes")
ubx.load_module(nd, "testtypes")

function test_scalar_assignment()
   local d=ubx.data_alloc(nd, "unsigned int")
   ubx.data_set(d, 33)
   local numptr = ffi.cast("unsigned int*", d.data)
   assert_equals(33, numptr[0])
   -- ubx.data_free(d)
end

function test_scalar_assignment_zero_data()
   local d=ubx.data_alloc(nd, "unsigned int", 0)
   ubx.data_set(d, 33, true)
   local numptr = ffi.cast("unsigned int*", d.data)
   assert_equals(33, numptr[0])
   -- ubx.data_free(d)
end

function test_string_assignment()
   local teststr="my beautiful string"
   local d=ubx.data_alloc(nd, "char", 30)
   ubx.data_set(d, teststr)
   local chrptr = ffi.cast("char*", d.data)
   assert_equals(teststr, ffi.string(chrptr))
   -- ubx.data_free(d)
end

function test_string_assignment_zero_data()
   local teststr="my beautiful string"
   local d=ubx.data_alloc(nd, "char", 0)
   ubx.data_set(d, teststr, true)
   local chrptr = ffi.cast("char*", d.data)
   assert_equals(teststr, ffi.string(chrptr))
   -- ubx.data_free(d)
end

function test_simple_struct_assignment()
   local d=ubx.data_alloc(nd, "struct kdl_vector")
   ubx.data_set(d, {x=444,y=55.3, z=-34})
   local vptr = ffi.cast("struct kdl_vector*", d.data)
   assert_equals(444, vptr.x)
   assert_equals(55.3, vptr.y)
   assert_equals(-34, vptr.z)
   -- ubx.data_free(d)
end

function test_simple_struct_assignment_zero_data()
   local d=ubx.data_alloc(nd, "struct kdl_vector", 0)
   ubx.data_set(d, {x=444,y=55.3, z=-34}, true)
   local vptr = ffi.cast("struct kdl_vector*", d.data)
   assert_equals(444, vptr.x)
   assert_equals(55.3, vptr.y)
   assert_equals(-34, vptr.z)
   -- ubx.data_free(d)
end

function test_composite_struct_assignment()
   local d=ubx.data_alloc(nd, "struct kdl_frame")
   local conf = {
      p={ x=444, y=55.3, z=-34 },
      M={ data={
	     [0]=1,   [1]=2,   [2]=3,
	     [3]=11,  [4]=22,  [5]=33,
	     [6]=111, [7]=222, [8]=333 }
      }
   }

   ubx.data_set(d, conf)
   local vptr = ffi.cast("struct kdl_frame*", d.data)
   assert_equals(444, vptr.p.x)
   assert_equals(55.3, vptr.p.y)
   assert_equals(-34, vptr.p.z)

   assert_equals(1, vptr.M.data[0])
   assert_equals(11, vptr.M.data[3])
   assert_equals(111, vptr.M.data[6])

   assert_equals(2, vptr.M.data[1])
   assert_equals(22, vptr.M.data[4])
   assert_equals(222, vptr.M.data[7])

   assert_equals(3, vptr.M.data[2])
   assert_equals(33, vptr.M.data[5])
   assert_equals(333, vptr.M.data[8])
end

function test_composite_struct_assignment_zero_data()
   local d=ubx.data_alloc(nd, "struct kdl_frame", 0)
   local conf = {
      p={ x=444, y=55.3, z=-34 },
      M={ data={
	     [0]=1,   [1]=2,   [2]=3,
	     [3]=11,  [4]=22,  [5]=33,
	     [6]=111, [7]=222, [8]=333 }
      }
   }

   ubx.data_set(d, conf, true)
   local vptr = ffi.cast("struct kdl_frame*", d.data)
   assert_equals(444, vptr.p.x)
   assert_equals(55.3, vptr.p.y)
   assert_equals(-34, vptr.p.z)

   assert_equals(1, vptr.M.data[0])
   assert_equals(11, vptr.M.data[3])
   assert_equals(111, vptr.M.data[6])

   assert_equals(2, vptr.M.data[1])
   assert_equals(22, vptr.M.data[4])
   assert_equals(222, vptr.M.data[7])

   assert_equals(3, vptr.M.data[2])
   assert_equals(33, vptr.M.data[5])
   assert_equals(333, vptr.M.data[8])
end

function test_simple_struct_assignment2()
   local d=ubx.data_alloc(nd, "struct test_trig_conf", 3)
   local conf = {
      { name="block_name1", benchmark=0 },
      { name="block_name2", benchmark=1 },
      { name="block_name3", benchmark=0 },
   }

   ubx.data_set(d, conf)

   local ptr = ffi.cast("struct test_trig_conf*", d.data)
   assert_equals("block_name1", ffi.string(ptr[0].name))
   assert_equals("block_name2", ffi.string(ptr[1].name))
   assert_equals("block_name3", ffi.string(ptr[2].name))

   assert_equals(0, tonumber(ptr[0].benchmark))
   assert_equals(1, tonumber(ptr[1].benchmark))
   assert_equals(0, tonumber(ptr[2].benchmark))

   -- ubx.data_free(d)
end

function test_data_resize()
   local d=ubx.data_alloc(nd, "struct test_trig_conf", 0)
   local conf = {
      { name="block_name1", benchmark=0 },
      { name="block_name2", benchmark=1 },
      { name="block_name3", benchmark=0 },
   }

   ubx.data_set(d, conf, true)

   local ptr = ffi.cast("struct test_trig_conf*", d.data)
   assert_equals("block_name1", ffi.string(ptr[0].name))
   assert_equals("block_name2", ffi.string(ptr[1].name))
   assert_equals("block_name3", ffi.string(ptr[2].name))

   assert_equals(0, tonumber(ptr[0].benchmark))
   assert_equals(1, tonumber(ptr[1].benchmark))
   assert_equals(0, tonumber(ptr[2].benchmark))

   assert_equals(tonumber(d.len), 3)

   -- ubx.data_free(d)
end

os.exit( lu.LuaUnit.run() )
