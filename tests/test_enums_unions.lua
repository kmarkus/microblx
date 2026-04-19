--
-- Tests for enum and union types in testtypes:
--   struct test_with_enum      (named enum field)
--   struct test_with_union     (named union)
--   struct test_with_anon_union (anonymous union, fields promoted to struct)
--   struct test_with_anon_enum  (anonymous enum field)
--
-- Each type is tested via:
--   a) USC config path  : bd.system + cconst, verify via :c("value"):tolua()
--   b) tolua default    : no struct2tab hook
--   c) tolua with hook  : custom struct2tab converter

local lu = require "luaunit"
local ffi = require "ffi"
local ubx = require "ubx"
local cdata = require "cdata"
local bd = require "blockdiagram"

local assert_equals = lu.assert_equals
local assert_true = lu.assert_true

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_WARN

------------------------------------------------------------------------------
-- USC config tests via blockdiagram + cconst
-- cconst adds a "value" config dynamically in init; blockdiagram's
-- reapply_config sets it after block init.
------------------------------------------------------------------------------

local sys_usc = bd.system {
   imports = { "stdtypes", "testtypes", "cconst" },
   blocks = {
      { name="c_enum",        type="ubx/cconst" },
      { name="c_enum_sym",    type="ubx/cconst" },   -- same type, symbolic value
      { name="c_union",       type="ubx/cconst" },
      { name="c_anon_union",  type="ubx/cconst" },
      { name="c_anon_enum",   type="ubx/cconst" },
      { name="c_anon_enum_sym", type="ubx/cconst" }, -- same type, symbolic value
   },
   configurations = {
      { name="c_enum",       config = { type_name="struct test_with_enum",
					value={ col=2, val=77 } } },
      { name="c_enum_sym",   config = { type_name="struct test_with_enum",
					value={ col="BLUE", val=77 } } },
      { name="c_union",      config = { type_name="struct test_with_union",
					value={ v={ i=99 }, tag=1 } } },
      { name="c_anon_union", config = { type_name="struct test_with_anon_union",
					value={ i=42, selector=3 } } },
      { name="c_anon_enum",  config = { type_name="struct test_with_anon_enum",
					value={ kind=1, value=55 } } },
      { name="c_anon_enum_sym", config = { type_name="struct test_with_anon_enum",
					   value={ kind="KIND_FLOAT", value=55 } } },
   },
}

TestEnumsUnionsUSC = {}

local nd_usc

function TestEnumsUnionsUSC.setupClass()
   nd_usc = sys_usc:launch{ nostart=true, loglevel=LOGLEVEL,
			     nodename="TestEnumsUnionsUSC" }
end

function TestEnumsUnionsUSC.teardownClass()
   if nd_usc then ubx.node_rm(nd_usc) end
   nd_usc = nil
end

function TestEnumsUnionsUSC:test_named_enum_cfg()
   local v = nd_usc:b("c_enum"):c("value"):tolua()
   assert_equals(v.col, 2,  "named enum: col mismatch")
   assert_equals(v.val, 77, "named enum: val mismatch")
end

-- LuaJIT FFI accepts symbolic enum strings; verify USC config works the same way
function TestEnumsUnionsUSC:test_named_enum_symbolic_cfg()
   local v = nd_usc:b("c_enum_sym"):c("value"):tolua()
   assert_equals(v.col, 2,  "named enum symbolic: 'BLUE' should equal 2")
   assert_equals(v.val, 77, "named enum symbolic: val mismatch")
end

function TestEnumsUnionsUSC:test_named_union_cfg()
   local v = nd_usc:b("c_union"):c("value"):tolua()
   assert_equals(v.v.i, 99, "named union: v.i mismatch")
   assert_equals(v.tag, 1,  "named union: tag mismatch")
end

function TestEnumsUnionsUSC:test_anon_union_cfg()
   local v = nd_usc:b("c_anon_union"):c("value"):tolua()
   assert_equals(v.i,        42, "anon union: i mismatch")
   assert_equals(v.selector,  3, "anon union: selector mismatch")
end

function TestEnumsUnionsUSC:test_anon_enum_cfg()
   local v = nd_usc:b("c_anon_enum"):c("value"):tolua()
   assert_equals(v.kind,  1,  "anon enum: kind mismatch")
   assert_equals(v.value, 55, "anon enum: value mismatch")
end

function TestEnumsUnionsUSC:test_anon_enum_symbolic_cfg()
   local v = nd_usc:b("c_anon_enum_sym"):c("value"):tolua()
   assert_equals(v.kind,  1,  "anon enum symbolic: 'KIND_FLOAT' should equal 1")
   assert_equals(v.value, 55, "anon enum symbolic: value mismatch")
end

------------------------------------------------------------------------------
-- tolua tests (default + struct2tab hook)
------------------------------------------------------------------------------

TestEnumsUnionsTolua = {}

local nd_tl

function TestEnumsUnionsTolua.setupClass()
   nd_tl = ubx.node_create("TestEnumsUnionsTolua")
   ubx.load_module(nd_tl, "stdtypes")
   ubx.load_module(nd_tl, "testtypes")
end

function TestEnumsUnionsTolua.teardownClass()
   if nd_tl then ubx.node_rm(nd_tl) end
   nd_tl = nil
end

-- named enum: default (no hook)
function TestEnumsUnionsTolua:test_named_enum_default()
   local d = ubx.data_alloc(nd_tl, "struct test_with_enum")
   ubx.data_set(d, { col=1, val=42 })
   local v = ubx.data_tolua(d)
   assert_equals(v.col, 1,  "named enum default: col mismatch")
   assert_equals(v.val, 42, "named enum default: val mismatch")
end

-- named enum: symbolic string value accepted by data_set (via LuaJIT FFI)
function TestEnumsUnionsTolua:test_named_enum_symbolic()
   local d = ubx.data_alloc(nd_tl, "struct test_with_enum")
   ubx.data_set(d, { col="GREEN", val=10 })
   local v = ubx.data_tolua(d)
   assert_equals(v.col, 1,  "named enum symbolic: 'GREEN' should equal 1")
   assert_equals(v.val, 10, "named enum symbolic: val mismatch")
end

-- named enum: struct2tab hook converts numeric col to string name
function TestEnumsUnionsTolua:test_named_enum_hook()
   local color_name = { [0]="RED", [1]="GREEN", [2]="BLUE" }
   cdata.struct2tab["struct test_with_enum"] = function(cd)
      return { col=color_name[tonumber(cd.col)], val=tonumber(cd.val) }
   end
   local d = ubx.data_alloc(nd_tl, "struct test_with_enum")
   ubx.data_set(d, { col=2, val=7 })
   local v = ubx.data_tolua(d)
   assert_equals(v.col, "BLUE", "named enum hook: col mismatch")
   assert_equals(v.val, 7,      "named enum hook: val mismatch")
   cdata.struct2tab["struct test_with_enum"] = nil
end

-- named union: default exposes all members as table
function TestEnumsUnionsTolua:test_named_union_default()
   local d = ubx.data_alloc(nd_tl, "struct test_with_union")
   ubx.data_set(d, { v={ i=77 }, tag=0 })
   local v = ubx.data_tolua(d)
   assert_equals(type(v.v), "table", "named union default: v should be table")
   assert_equals(v.v.i, 77,          "named union default: v.i mismatch")
   assert_equals(v.tag, 0,           "named union default: tag mismatch")
end

-- named union: struct2tab hook on the union selects only the active member
function TestEnumsUnionsTolua:test_named_union_hook()
   cdata.struct2tab["union test_variant"] = function(cd)
      return tonumber(cd.i)
   end
   local d = ubx.data_alloc(nd_tl, "struct test_with_union")
   ubx.data_set(d, { v={ i=55 }, tag=2 })
   local v = ubx.data_tolua(d)
   assert_equals(v.v,  55, "named union hook: v mismatch")
   assert_equals(v.tag, 2, "named union hook: tag mismatch")
   cdata.struct2tab["union test_variant"] = nil
end

-- anonymous union: default promotes sub-fields to parent struct
function TestEnumsUnionsTolua:test_anon_union_default()
   local d = ubx.data_alloc(nd_tl, "struct test_with_anon_union")
   ubx.data_set(d, { i=100, selector=1 })
   local v = ubx.data_tolua(d)
   assert_equals(v.i,        100, "anon union default: i mismatch")
   assert_equals(v.selector,   1, "anon union default: selector mismatch")
   assert_true(v.f ~= nil,        "anon union default: f should be present")
end

-- anonymous union: struct2tab hook on the containing struct
function TestEnumsUnionsTolua:test_anon_union_hook()
   cdata.struct2tab["struct test_with_anon_union"] = function(cd)
      return { i=tonumber(cd.i), selector=tonumber(cd.selector) }
   end
   local d = ubx.data_alloc(nd_tl, "struct test_with_anon_union")
   ubx.data_set(d, { i=200, selector=0 })
   local v = ubx.data_tolua(d)
   assert_equals(v.i,        200, "anon union hook: i mismatch")
   assert_equals(v.selector,   0, "anon union hook: selector mismatch")
   assert_equals(v.f,        nil, "anon union hook: f should be suppressed")
   cdata.struct2tab["struct test_with_anon_union"] = nil
end

-- anonymous enum: symbolic string value accepted by data_set
function TestEnumsUnionsTolua:test_anon_enum_symbolic()
   local d = ubx.data_alloc(nd_tl, "struct test_with_anon_enum")
   ubx.data_set(d, { kind="KIND_FLOAT", value=5 })
   local v = ubx.data_tolua(d)
   assert_equals(v.kind,  1, "anon enum symbolic: 'KIND_FLOAT' should equal 1")
   assert_equals(v.value, 5, "anon enum symbolic: value mismatch")
end

-- anonymous enum field: default converts to number
function TestEnumsUnionsTolua:test_anon_enum_default()
   local d = ubx.data_alloc(nd_tl, "struct test_with_anon_enum")
   ubx.data_set(d, { kind=1, value=33 })
   local v = ubx.data_tolua(d)
   assert_equals(v.kind,  1,  "anon enum default: kind mismatch")
   assert_equals(v.value, 33, "anon enum default: value mismatch")
end

-- anonymous enum field: hook converts numeric kind to string
function TestEnumsUnionsTolua:test_anon_enum_hook()
   cdata.struct2tab["struct test_with_anon_enum"] = function(cd)
      local names = { [0]="KIND_INT", [1]="KIND_FLOAT" }
      return { kind=names[tonumber(cd.kind)], value=tonumber(cd.value) }
   end
   local d = ubx.data_alloc(nd_tl, "struct test_with_anon_enum")
   ubx.data_set(d, { kind=0, value=11 })
   local v = ubx.data_tolua(d)
   assert_equals(v.kind,  "KIND_INT", "anon enum hook: kind mismatch")
   assert_equals(v.value, 11,         "anon enum hook: value mismatch")
   cdata.struct2tab["struct test_with_anon_enum"] = nil
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
