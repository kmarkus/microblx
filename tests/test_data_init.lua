ffi=require"ffi"
lunit=require"lunit"
ubx=require"ubx"

module("data_init_test", lunit.testcase, package.seeall)

local ni=ubx.node_create("data_init_test")

ubx.load_module(ni, "std_types/stdtypes/stdtypes.so")
ubx.load_module(ni, "std_types/testtypes/testtypes.so")
ubx.load_module(ni, "std_types/kdl/kdl_types.so")

function test_scalar_assignment()
   local d=ubx.data_alloc(ni, "unsigned int")
   ubx.data_set(d, 33)
   local numptr = ffi.cast("unsigned int*", d.data)
   assert_equal(33, numptr[0])
   -- ubx.data_free(ni, d)
end

function test_string_assignment()
   local teststr="my beautiful string"
   local d=ubx.data_alloc(ni, "char", 30)
   ubx.data_set(d, teststr)
   local chrptr = ffi.cast("char*", d.data)
   assert_equal(teststr, ffi.string(chrptr))
   -- ubx.data_free(ni, d)
end

function test_simple_struct_assignment()
   local d=ubx.data_alloc(ni, "kdl/struct Vector")
   ubx.data_set(d, {x=444,y=55.3, z=-34})
   local vptr = ffi.cast("struct Vector*", d.data)
   assert_equal(444, vptr.x)
   assert_equal(55.3, vptr.y)
   assert_equal(-34, vptr.z)
   -- ubx.data_free(ni, d)
end

function test_composite_struct_assignment()
   local d=ubx.data_alloc(ni, "kdl/struct Frame")
   local conf = {
      p={ x=444, y=55.3, z=-34 },
      M={ data={ 
	     [0]=1,   [1]=2,   [2]=3,
	     [3]=11,  [4]=22,  [5]=33,
	     [6]=111, [7]=222, [8]=333 }
      }
   }

   ubx.data_set(d, conf)
   local vptr = ffi.cast("struct Frame*", d.data)
   assert_equal(444, vptr.p.x)
   assert_equal(55.3, vptr.p.y)
   assert_equal(-34, vptr.p.z)

   assert_equal(1, vptr.M.data[0])
   assert_equal(11, vptr.M.data[3])
   assert_equal(111, vptr.M.data[6])

   assert_equal(2, vptr.M.data[1])
   assert_equal(22, vptr.M.data[4])
   assert_equal(222, vptr.M.data[7])

   assert_equal(3, vptr.M.data[2])
   assert_equal(33, vptr.M.data[5])
   assert_equal(333, vptr.M.data[8])
end

function test_simple_struct_assignment2()
   local d=ubx.data_alloc(ni, "testtypes/struct test_trig_conf", 3)
   local conf = {
      { name="block_name1", benchmark=0 },
      { name="block_name2", benchmark=1 },
      { name="block_name3", benchmark=0 },
   }

   ubx.data_set(d, conf)

   local ptr = ffi.cast("struct test_trig_conf*", d.data)
   assert_equal("block_name1", ffi.string(ptr[0].name))
   assert_equal("block_name2", ffi.string(ptr[1].name))
   assert_equal("block_name3", ffi.string(ptr[2].name))

   assert_equal(0, tonumber(ptr[0].benchmark))
   assert_equal(1, tonumber(ptr[1].benchmark))
   assert_equal(0, tonumber(ptr[2].benchmark))

   -- ubx.data_free(ni, d)
end

function test_data_resize()
   local d=ubx.data_alloc(ni, "testtypes/struct test_trig_conf", 1)
   local conf = {
      { name="block_name1", benchmark=0 },
      { name="block_name2", benchmark=1 },
      { name="block_name3", benchmark=0 },
   }

   ubx.data_set(d, conf, true)

   local ptr = ffi.cast("struct test_trig_conf*", d.data)
   assert_equal("block_name1", ffi.string(ptr[0].name))
   assert_equal("block_name2", ffi.string(ptr[1].name))
   assert_equal("block_name3", ffi.string(ptr[2].name))

   assert_equal(0, tonumber(ptr[0].benchmark))
   assert_equal(1, tonumber(ptr[1].benchmark))
   assert_equal(0, tonumber(ptr[2].benchmark))

   assert_equal(d.len, 3)

   -- ubx.data_free(ni, d)
end
