ffi=require"ffi"
lunit=require"lunit"
u5c=require"u5c"

module("data_init_test", lunit.testcase, package.seeall)

ni=u5c.node_create("unit_test_node")
u5c.load_module(ni, "std_types/stdtypes/stdtypes.so")
u5c.load_module(ni, "std_types/testtypes/testtypes.so")
u5c.ffi_load_types(ni)

function test_scalar_assignment()
   local d=u5c.data_alloc(ni, "unsigned int")
   u5c.data_set(d, 33)
   local numptr = ffi.cast("unsigned int*", d.data)
   assert_equal(33, numptr[0])
   u5c.data_free(ni, d)
end

function test_string_assignment()
   local teststr="my beautiful string"
   local d=u5c.data_alloc(ni, "char", 30)
   u5c.data_set(d, teststr)
   local chrptr = ffi.cast("char*", d.data)
   assert_equal(teststr, ffi.string(chrptr))
   u5c.data_free(ni, d)
end

function test_simple_struct_assignment()
   local d=u5c.data_alloc(ni, "testtypes/struct Vector")
   u5c.data_set(d, {x=444,y=55.3, z=-34})
   local vptr = ffi.cast("struct Vector*", d.data)
   assert_equal(444, vptr.x)
   assert_equal(55.3, vptr.y)
   assert_equal(-34, vptr.z)
   u5c.data_free(ni, d)
end

function test_composite_struct_assignment()
   local d=u5c.data_alloc(ni, "testtypes/struct Frame")
   local conf = {
      p={ x=444, y=55.3, z=-34 },
      M={ X={ x=1, y=2, z=3 },
	  Y={ x=11, y=22, z=33 },
	  Z={ x=111, y=222, z=333 },
       }
   }

   u5c.data_set(d, conf)
   local vptr = ffi.cast("struct Frame*", d.data)
   assert_equal(444, vptr.p.x)
   assert_equal(55.3, vptr.p.y)
   assert_equal(-34, vptr.p.z)

   assert_equal(1, vptr.M.X.x)
   assert_equal(2, vptr.M.X.y)
   assert_equal(3, vptr.M.X.z)

   assert_equal(11, vptr.M.Y.x)
   assert_equal(22, vptr.M.Y.y)
   assert_equal(33, vptr.M.Y.z)

   assert_equal(111, vptr.M.Z.x)
   assert_equal(222, vptr.M.Z.y)
   assert_equal(333, vptr.M.Z.z)
end

function test_simple_struct_assignment2()
   local d=u5c.data_alloc(ni, "testtypes/struct test_trig_conf", 3)
   local conf = {
      { name="block_name1", benchmark=0 },
      { name="block_name2", benchmark=1 },
      { name="block_name3", benchmark=0 },
   }

   u5c.data_set(d, conf)

   local ptr = ffi.cast("struct test_trig_conf*", d.data)
   assert_equal("block_name1", ffi.string(ptr[0].name))
   assert_equal("block_name2", ffi.string(ptr[1].name))
   assert_equal("block_name3", ffi.string(ptr[2].name))

   assert_equal(0, tonumber(ptr[0].benchmark))
   assert_equal(1, tonumber(ptr[1].benchmark))
   assert_equal(0, tonumber(ptr[2].benchmark))

   u5c.data_free(ni, d)
end

function test_data_resize()
   local d=u5c.data_alloc(ni, "testtypes/struct test_trig_conf", 1)
   local conf = {
      { name="block_name1", benchmark=0 },
      { name="block_name2", benchmark=1 },
      { name="block_name3", benchmark=0 },
   }

   u5c.data_set(d, conf, true)

   local ptr = ffi.cast("struct test_trig_conf*", d.data)
   assert_equal("block_name1", ffi.string(ptr[0].name))
   assert_equal("block_name2", ffi.string(ptr[1].name))
   assert_equal("block_name3", ffi.string(ptr[2].name))

   assert_equal(0, tonumber(ptr[0].benchmark))
   assert_equal(1, tonumber(ptr[1].benchmark))
   assert_equal(0, tonumber(ptr[2].benchmark))

   assert_equal(d.len, 3)

   u5c.data_free(ni, d)
end
