--
-- Test runtime (struct) type registration from Lua: ubx.type_add /
-- ubx.type_rm. See docs/dev/005-luablock-runtime-types.md.
--

local lu = require("luaunit")
local ubx = require("ubx")
local ffi = require("ffi")

local PT      = "struct ttadd_point"
local PT_DECL = "struct ttadd_point { double x; double y; int32_t n; };"

local NI

TestTypeAdd = {}

function TestTypeAdd:setup()
   NI = ubx.node_create("TestTypeAdd", { loglevel = ffi.C.UBX_LOGLEVEL_WARN })
   ubx.load_module(NI, "stdtypes")
end

function TestTypeAdd:teardown()
   if NI then
      pcall(ubx.type_rm, NI, PT)  -- drop the runtime type if still present
      ubx.node_rm(NI)
   end
   NI = nil
end

--- a registered type is a real, fully-formed ubx_type_t
function TestTypeAdd:test_register_basic()
   local t = ubx.type_add(NI, PT, PT_DECL)
   lu.assert_not_nil(t)

   -- looked up by name it is the same type
   lu.assert_true(ubx.type_get(NI, PT) == t)
   -- size and class are correct
   lu.assert_equals(tonumber(t.size), tonumber(ffi.sizeof(PT)))
   lu.assert_equals(t.type_class, ffi.C.TYPE_CLASS_STRUCT)
   -- the name-based hash was computed and resolves back to the type
   lu.assert_true(ubx.type_get_by_hash(NI, t.hash) == t)
   -- private_data carries the cdecl
   lu.assert_equals(ffi.string(ffi.cast("char*", t.private_data)), PT_DECL)
end

--- the type is usable like any other: allocate + marshal a Lua table
function TestTypeAdd:test_usable_as_data()
   ubx.type_add(NI, PT, PT_DECL)

   local d = ubx.data_alloc(NI, PT)
   ubx.data_set(d, { x = 1.5, y = 2.5, n = 7 })

   -- read back through the ffi ...
   local p = ffi.cast(PT .. "*", d.data)
   lu.assert_almost_equals(p.x, 1.5, 1e-9)
   lu.assert_almost_equals(p.y, 2.5, 1e-9)
   lu.assert_equals(p.n, 7)

   -- ... and through data_tolua
   local v = ubx.data_tolua(d)
   lu.assert_almost_equals(v.x, 1.5, 1e-9)
   lu.assert_almost_equals(v.y, 2.5, 1e-9)
   lu.assert_equals(v.n, 7)
end

--- re-registering the same name in a node returns the existing type
function TestTypeAdd:test_idempotent()
   local t1 = ubx.type_add(NI, PT, PT_DECL)
   local t2 = ubx.type_add(NI, PT, PT_DECL)
   lu.assert_true(t1 == t2)
end

--- an optional doc string is stored
function TestTypeAdd:test_doc()
   local t = ubx.type_add(NI, PT, PT_DECL, "a 2D point with a counter")
   lu.assert_equals(ffi.string(t.doc), "a 2D point with a counter")
end

--- type_rm unregisters and is idempotent-ish (false on a second call)
function TestTypeAdd:test_rm()
   ubx.type_add(NI, PT, PT_DECL)
   lu.assert_true(ubx.type_get(NI, PT) ~= nil)

   lu.assert_true(ubx.type_rm(NI, PT))
   lu.assert_nil(ubx.type_get(NI, PT))

   lu.assert_false(ubx.type_rm(NI, PT))  -- already gone
end

--- a type can be added again after removal (fresh registration)
function TestTypeAdd:test_readd_after_rm()
   ubx.type_add(NI, PT, PT_DECL)
   ubx.type_rm(NI, PT)
   local t = ubx.type_add(NI, PT, PT_DECL)
   lu.assert_true(ubx.type_get(NI, PT) == t)
end

--- type_rm refuses to touch a static (module) type -- not ours to free
function TestTypeAdd:test_rm_static_refused()
   lu.assert_not_nil(ubx.type_get(NI, "double"))  -- from stdtypes
   lu.assert_false(pcall(ubx.type_rm, NI, "double"))
   lu.assert_not_nil(ubx.type_get(NI, "double"))  -- still there
end

--- bad arguments are rejected
function TestTypeAdd:test_bad_args()
   lu.assert_false(pcall(ubx.type_add, NI, "", PT_DECL))          -- empty name
   lu.assert_false(pcall(ubx.type_add, NI, PT, ""))               -- empty cdecl
   lu.assert_false(pcall(ubx.type_add, nil, PT, PT_DECL))         -- nil node
end

--- a cdecl that does not define `name` fails cleanly (nothing registered)
function TestTypeAdd:test_cdecl_mismatch()
   lu.assert_false(pcall(ubx.type_add, NI, "struct ttadd_nope",
			 "struct ttadd_other { int a; };"))
   lu.assert_nil(ubx.type_get(NI, "struct ttadd_nope"))
end

if not _RUNNER then
   os.exit(lu.LuaUnit.run())
end
