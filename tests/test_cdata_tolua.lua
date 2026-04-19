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

local nd

TestCdataTolua = {}

function TestCdataTolua.setupClass()
   nd=ubx.node_create("cdata_tolua_test")
   ubx.load_module(nd, "stdtypes")
   ubx.load_module(nd, "testtypes")
end

function TestCdataTolua.teardownClass()
   if nd then ubx.node_rm(nd) end
   nd = nil
end

function TestCdataTolua:test_vector()
   local init = {x=1,y=2,z=3}
   local v1 = ffi.new("struct kdl_vector", init)

   local val=cdata.tolua(v1)
   assert_true(utils.table_cmp(val, init), "A: table->vector rountrip comparison error")
end

function TestCdataTolua:test_vector_inv()
   local init = {x=1,y=2,z=3}
   local v1 = ffi.new("struct kdl_vector", init)
   v1.x=33
   v1.z=55
   local val=cdata.tolua(v1)
   assert_false(utils.table_cmp(val, init), "B: table->vector rountrip comparison error")
end

function TestCdataTolua:test_frame()
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

function TestCdataTolua:test_frame_inv()
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

function TestCdataTolua:test_char()
   local init = {
      name="Frodo Baggins",
      benchmark=993
   }

   local c = ffi.new("struct test_trig_conf", init)
   local val=cdata.tolua(c)
   assert_true(utils.table_cmp(val, init), "E: table->char rountrip comparison error")
end


function TestCdataTolua:test_int()
   local init=33
   local i = ffi.new("unsigned int", init)
   local val=cdata.tolua(i)
   assert_equals(val, init, "F: table->int rountrip comparison error")
end

function TestCdataTolua:test_int_inv()
   local init=33
   local i = ffi.new("unsigned int", init)
   local val=cdata.tolua(i)
   assert_not_equals(init, val-1, "G: table->int rountrip comparison error")
end

function TestCdataTolua:test_ubx_data()
   local ubx_data_vect = ubx.data_alloc(nd, "struct kdl_vector", 1)
   local init = { x=7, y=8, z=9 }
   ubx.data_set(ubx_data_vect, init, true)
   local val = ubx.data_tolua(ubx_data_vect)
   assert_true(utils.table_cmp(val, init), "H: table->ubx_data(vector) rountrip comparison error")
end

function TestCdataTolua:test_ubx_data_inv()
   local ubx_data_vect = ubx.data_alloc(nd, "struct kdl_vector", 1)
   local init = { x=2, y=5, z=22 }
   ubx.data_set(ubx_data_vect, init, true)
   init.x=344
   local val = ubx.data_tolua(ubx_data_vect)
   assert_false(utils.table_cmp(val, init), "I: table->ubx_data(vector) rountrip comparison error")
end

function TestCdataTolua:test_ubx_data_basic()
   local ubx_data_int = ubx.data_alloc(nd, "unsigned int", 1)
   local init = 4711
   ubx.data_set(ubx_data_int, init, false)

   local val = ubx.data_tolua(ubx_data_int)
   assert_equals(init, val, "J: table->ubx_data(int) rountrip comparison error")
end

function TestCdataTolua:test_pointer_to_prim()
   local init = 333
   local i = ffi.new("unsigned int[1]", init )
   local ip = ffi.new("unsigned int*", i)

   assert_equals(init, tonumber(i[0]), "K: init failed")
   assert_equals(tonumber(i[0]), tonumber(ip[0]), "L: mismatch between int and int*")

   local val = cdata.tolua(ip)

   assert_equals(init, val, "L: mismatch after converting from cdata")

end

function TestCdataTolua:test_arr_data()
   local d = ubx.data_alloc(nd, "double", 5)
   local init = {1.1,2.2,3.3,4.4,5.5}
   ubx.data_set(d, init)
   local res = ubx.data_tolua(d)
   assert_true(utils.table_cmp(res, init), "test_arr_data double[5] roundtrip failed")
end

function TestCdataTolua:test_void_pointer_to_prim()
   local init = 0xdeafbeaf
   local vp = ffi.new("void *", ffi.cast('void *', init))
   local val = cdata.tolua(vp)
   assert_equals(init, val, "L: mismatch after converting from cdata")
end

--- enum and union support -------------------------------------------------
-- types are defined by the testtypes module loaded in setupClass

function TestCdataTolua:test_enum_basic()
   local e = ffi.new("enum test_color", "GREEN")
   local val = cdata.tolua(e)
   assert_equals(val, 1, "enum basic value mismatch")
end

function TestCdataTolua:test_enum_in_struct()
   local s = ffi.new("struct test_with_enum", { col = "BLUE", val = 42 })
   local val = cdata.tolua(s)
   assert_equals(val.col, 2, "enum-in-struct color mismatch")
   assert_equals(val.val, 42, "enum-in-struct val mismatch")
end

function TestCdataTolua:test_union_default()
   local u = ffi.new("union test_variant")
   u.i = 77
   local val = cdata.tolua(u)
   -- default (no hook): all members converted like a struct
   assert_equals(type(val), "table", "union should convert to table")
   assert_equals(val.i, 77, "union member i mismatch")
   -- val.f is whatever the bit-pattern gives; just check it exists
   assert_true(val.f ~= nil, "union member f should be present")
end

function TestCdataTolua:test_union_in_struct()
   local s = ffi.new("struct test_with_union")
   s.v.i = 99
   s.tag = 1
   local val = cdata.tolua(s)
   assert_equals(type(val.v), "table", "nested union should be a table")
   assert_equals(val.v.i, 99, "nested union member mismatch")
   assert_equals(val.tag, 1, "struct tag mismatch")
end

function TestCdataTolua:test_union_struct2tab_hook()
   -- register a custom converter for the named union
   cdata.struct2tab['union test_variant'] = function(cd)
      return { i = tonumber(cd.i) }
   end

   local u = ffi.new("union test_variant")
   u.i = 123
   local val = cdata.tolua(u)
   assert_equals(type(val), "table", "hooked union should return table")
   assert_equals(val.i, 123, "hooked union value mismatch")
   assert_equals(val.f, nil, "hook should suppress member f")

   -- cleanup
   cdata.struct2tab['union test_variant'] = nil
end

function TestCdataTolua:test_struct_struct2tab_hook()
   -- verify the existing struct hook mechanism still works
   cdata.struct2tab['struct test_with_enum'] = function(cd)
      return "custom"
   end

   local s = ffi.new("struct test_with_enum", { col = "RED", val = 7 })
   local val = cdata.tolua(s)
   assert_equals(val, "custom", "struct hook should override default")

   -- cleanup
   cdata.struct2tab['struct test_with_enum'] = nil
end

function TestCdataTolua:test_enum_destruct()
   local res = cdata.ctype_destruct(ffi.typeof("struct test_with_enum"))
   assert_equals(res.col, "number", "enum field should destruct to 'number'")
   assert_equals(res.val, "number", "int field should destruct to 'number'")
end

function TestCdataTolua:test_union_destruct()
   local res = cdata.ctype_destruct(ffi.typeof("union test_variant"))
   assert_equals(res.i, "number", "union int member should be 'number'")
   assert_equals(res.f, "number", "union float member should be 'number'")
end

if not _RUNNER then os.exit( lu.LuaUnit.run() ) end
