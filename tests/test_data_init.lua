local lu = require"luaunit"
local ffi=require"ffi"
local ubx=require"ubx"

local assert_equals = lu.assert_equals

local nd

TestDataInit = {}

function TestDataInit.setupClass()
   nd=ubx.node_create("data_init_test")
   ubx.load_module(nd, "stdtypes")
   ubx.load_module(nd, "testtypes")
end

function TestDataInit.teardownClass()
   if nd then ubx.node_rm(nd) end
   nd = nil
end

function TestDataInit:test_scalar_assignment()
   local d=ubx.data_alloc(nd, "unsigned int")
   ubx.data_set(d, 33)
   local numptr = ffi.cast("unsigned int*", d.data)
   assert_equals(33, numptr[0])
   -- ubx.data_free(d)
end

function TestDataInit:test_scalar_assignment_zero_data()
   local d=ubx.data_alloc(nd, "unsigned int", 0)
   ubx.data_set(d, 33, true)
   local numptr = ffi.cast("unsigned int*", d.data)
   assert_equals(33, numptr[0])
   -- ubx.data_free(d)
end

function TestDataInit:test_string_assignment()
   local teststr="my beautiful string"
   local d=ubx.data_alloc(nd, "char", 30)
   ubx.data_set(d, teststr)
   local chrptr = ffi.cast("char*", d.data)
   assert_equals(teststr, ffi.string(chrptr))
   -- ubx.data_free(d)
end

function TestDataInit:test_string_assignment_zero_data()
   local teststr="my beautiful string"
   local d=ubx.data_alloc(nd, "char", 0)
   ubx.data_set(d, teststr, true)
   local chrptr = ffi.cast("char*", d.data)
   assert_equals(teststr, ffi.string(chrptr))
   -- ubx.data_free(d)
end

function TestDataInit:test_simple_struct_assignment()
   local d=ubx.data_alloc(nd, "struct kdl_vector")
   ubx.data_set(d, {x=444,y=55.3, z=-34})
   local vptr = ffi.cast("struct kdl_vector*", d.data)
   assert_equals(444, vptr.x)
   assert_equals(55.3, vptr.y)
   assert_equals(-34, vptr.z)
   -- ubx.data_free(d)
end

function TestDataInit:test_simple_struct_assignment_zero_data()
   local d=ubx.data_alloc(nd, "struct kdl_vector", 0)
   ubx.data_set(d, {x=444,y=55.3, z=-34}, true)
   local vptr = ffi.cast("struct kdl_vector*", d.data)
   assert_equals(444, vptr.x)
   assert_equals(55.3, vptr.y)
   assert_equals(-34, vptr.z)
   -- ubx.data_free(d)
end

function TestDataInit:test_composite_struct_assignment()
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

function TestDataInit:test_composite_struct_assignment_zero_data()
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

function TestDataInit:test_simple_struct_assignment2()
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

function TestDataInit:test_data_resize()
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

-- Integer cdata input. data_set accepts int8..int64 / uint8..uint64
-- cdata as scalar input and lets LuaJIT FFI convert to the target
-- ctype on assignment (truncation / widening / signed-unsigned),
-- mirroring the Lua-number path.

function TestDataInit:test_cdata_uint64_into_uint64()
   local d=ubx.data_alloc(nd, "uint64_t")
   ubx.data_set(d, ffi.new("uint64_t", 0xdeadbeef))
   local p = ffi.cast("uint64_t*", d.data)
   assert_equals(0xdeadbeefULL, p[0])
end

function TestDataInit:test_cdata_int64_into_int64()
   local d=ubx.data_alloc(nd, "int64_t")
   ubx.data_set(d, ffi.new("int64_t", -42))
   local p = ffi.cast("int64_t*", d.data)
   assert_equals(-42LL, p[0])
end

function TestDataInit:test_cdata_uint64_into_double()
   -- this is the microblx test_set_get_config case: a JSON 42 decoded
   -- as 42ULL must be assignable to a double slot.
   local d=ubx.data_alloc(nd, "double")
   ubx.data_set(d, ffi.new("uint64_t", 42))
   local p = ffi.cast("double*", d.data)
   assert_equals(42, p[0])
end

function TestDataInit:test_cdata_int64_into_int()
   local d=ubx.data_alloc(nd, "int")
   ubx.data_set(d, ffi.new("int64_t", -1234))
   local p = ffi.cast("int*", d.data)
   assert_equals(-1234, p[0])
end

function TestDataInit:test_cdata_uint64_into_uint32()
   local d=ubx.data_alloc(nd, "uint32_t")
   ubx.data_set(d, ffi.new("uint64_t", 0xcafebabe))
   local p = ffi.cast("uint32_t*", d.data)
   assert_equals(0xcafebabe, tonumber(p[0]))
end

function TestDataInit:test_cdata_truncates_into_byte()
   -- 0x1FF -> 0xFF, mirroring the Lua-number truncation behavior
   local d=ubx.data_alloc(nd, "uint8_t")
   ubx.data_set(d, ffi.new("uint64_t", 0x1FF))
   local p = ffi.cast("uint8_t*", d.data)
   assert_equals(0xFF, p[0])
end

function TestDataInit:test_cdata_int8_round_trip()
   local d=ubx.data_alloc(nd, "int8_t")
   ubx.data_set(d, ffi.new("int8_t", -5))
   local p = ffi.cast("int8_t*", d.data)
   assert_equals(-5, p[0])
end

function TestDataInit:test_cdata_into_zero_data_resizes()
   local d=ubx.data_alloc(nd, "uint32_t", 0)
   ubx.data_set(d, ffi.new("uint64_t", 99), true)
   local p = ffi.cast("uint32_t*", d.data)
   assert_equals(99, tonumber(p[0]))
   assert_equals(1, tonumber(d.len))
end

function TestDataInit:test_cdata_into_zero_data_no_resize_errors()
   local d=ubx.data_alloc(nd, "uint32_t", 0)
   local ok, err = pcall(ubx.data_set, d, ffi.new("uint64_t", 99))
   lu.assert_false(ok)
   lu.assert_str_contains(err, "can't assign scalar")
end

function TestDataInit:test_non_numeric_cdata_rejected()
   local d=ubx.data_alloc(nd, "uint32_t")
   local ok, err = pcall(ubx.data_set, d, ffi.new("double", 1.5))
   lu.assert_false(ok)
   lu.assert_str_contains(err, "don't know how to assign")
end

function TestDataInit:test_pointer_cdata_rejected()
   local d=ubx.data_alloc(nd, "uint32_t")
   local ok, err = pcall(ubx.data_set, d, ffi.new("void *", nil))
   lu.assert_false(ok)
   lu.assert_str_contains(err, "don't know how to assign")
end

if not _RUNNER then os.exit( lu.LuaUnit.run() ) end
