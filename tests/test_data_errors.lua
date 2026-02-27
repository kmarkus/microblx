--
-- Test data_set and data_alloc error paths
--

local lu = require("luaunit")
local ffi = require("ffi")
local ubx = require("ubx")

local assert_equals = lu.assert_equals
local assert_true = lu.assert_true
local assert_false = lu.assert_false

local nd

TestDataErrors = {}

function TestDataErrors.setupClass()
   nd = ubx.node_create("data_errors_test", { loglevel = ffi.C.UBX_LOGLEVEL_WARN })
   ubx.load_module(nd, "stdtypes")
   ubx.load_module(nd, "testtypes")
end

function TestDataErrors.teardownClass()
   if nd then ubx.node_rm(nd) end
   nd = nil
end

--- Test data_set table beyond bounds without resize raises error
function TestDataErrors:test_data_set_beyond_bounds_no_resize()
   local d = ubx.data_alloc(nd, "unsigned int", 2)
   local ok, err = pcall(ubx.data_set, d, { 1, 2, 3 }, false)
   assert_false(ok, "expected error when writing beyond bounds without resize")
end

--- Test data_set with resize=true grows the data
function TestDataErrors:test_data_set_beyond_bounds_with_resize()
   local d = ubx.data_alloc(nd, "unsigned int", 1)
   ubx.data_set(d, { 10, 20, 30 }, true)
   assert_equals(tonumber(d.len), 3)
   local val = ubx.data_tolua(d)
   assert_equals(val, { 10, 20, 30 })
end

--- Test data_set scalar to array without resize raises error
function TestDataErrors:test_data_set_scalar_to_array_no_resize()
   local d = ubx.data_alloc(nd, "unsigned int", 3)
   local ok, err = pcall(ubx.data_set, d, 42, false)
   assert_false(ok, "expected error when assigning scalar to array without resize")
end

--- Test data_set scalar to array with resize works
function TestDataErrors:test_data_set_scalar_to_array_with_resize()
   local d = ubx.data_alloc(nd, "unsigned int", 3)
   ubx.data_set(d, 42, true)
   assert_equals(tonumber(d.len), 1)
   assert_equals(ubx.data_tolua(d), 42)
end

--- Test data_isnull
function TestDataErrors:test_data_isnull()
   local d = ubx.data_alloc(nd, "unsigned int", 0)
   assert_true(ubx.data_isnull(d))

   ubx.data_set(d, 99, true)
   assert_false(ubx.data_isnull(d))
end

--- Test data_alloc with unknown type raises error
function TestDataErrors:test_data_alloc_unknown_type()
   local ok, err = pcall(ubx.data_alloc, nd, "__nonexistent_type__", 1)
   assert_false(ok, "expected error for unknown type")
end

--- Test data_tolua with nil raises error
function TestDataErrors:test_data_tolua_nil()
   local ok, err = pcall(ubx.data_tolua, nil)
   assert_false(ok, "expected error for nil data")
end

--- Test data_resize
function TestDataErrors:test_data_resize()
   local d = ubx.data_alloc(nd, "double", 2)
   ubx.data_set(d, { 1.1, 2.2 })
   assert_equals(tonumber(d.len), 2)

   assert_true(ubx.data_resize(d, 5))
   assert_equals(tonumber(d.len), 5)
end

--- Test data_set struct to zero-len data with resize
function TestDataErrors:test_data_set_struct_zero_len_resize()
   local d = ubx.data_alloc(nd, "struct kdl_vector", 0)
   assert_true(ubx.data_isnull(d))

   ubx.data_set(d, { x = 1, y = 2, z = 3 }, true)
   assert_false(ubx.data_isnull(d))

   local val = ubx.data_tolua(d)
   assert_equals(val.x, 1)
   assert_equals(val.y, 2)
   assert_equals(val.z, 3)
end

--- Test data_set string that exceeds initial len auto-resizes
function TestDataErrors:test_data_set_string_auto_resize()
   local d = ubx.data_alloc(nd, "char", 5)
   local longstr = "this is a longer string than 5 chars"
   ubx.data_set(d, longstr)
   local chrptr = ffi.cast("char*", d.data)
   assert_equals(longstr, ffi.string(chrptr))
end

--- Test data OO methods via metatype
function TestDataErrors:test_data_oo_methods()
   local d = ubx.data_alloc(nd, "unsigned int", 1)
   d:set(777)
   assert_equals(d:tolua(), 777)
   assert_equals(d:size(), ffi.sizeof("unsigned int"))
   assert_false(d:isnull())
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
