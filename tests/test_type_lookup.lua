
local luaunit = require("luaunit")
local ubx = require("ubx")
local ffi = require("ffi")
local utils = require("utils")

local assert_equals = luaunit.assert_equals

local NI

TestTypeLookup = {}

function TestTypeLookup:setup()
   NI =	ubx.node_create("TestTypeLookup", { loglevel=7 } )
   ubx.load_module(NI, "stdtypes")
end

function TestTypeLookup:teardown()
   if NI then ubx.node_cleanup(NI) end
   NI = nil
end

function TestTypeLookup:TestLookupIntByHash()
   local t1 = ubx.type_get(NI, "double")
   luaunit.assert_not_nil(t1, "failed to lookup type 'double'")
   local t2 = ubx.type_get_by_hash(NI, t1.hash)
   luaunit.assert_not_nil(t2, "failed to lookup type 'double' by hash")
   luaunit.assertTrue(t1 == t2, "type 'double' looked up via name and hash not equal" )
end

function TestTypeLookup:TestLookupIntByHashstr()
   local t1_hstr = ffi.new("char["..tostring(ffi.C.TYPE_HASHSTR_LEN+1)..']')
   local t1 = ubx.type_get(NI, "int")
   luaunit.assert_not_nil(t1, "failed to lookup type 'int'")
   ubx.type_hashstr(t1, t1_hstr)
   luaunit.assert_not_nil(t1_hstr, "failed to retrieve hashstr for 'int'")
   local t2 = ubx.type_get_by_hashstr(NI, ffi.string(t1_hstr))
   luaunit.assert_not_nil(t2, "failed to lookup type 'int' by hashstr")
   luaunit.assertTrue(t1 == t2, "type 'int' looked up via name and hashstr not equal" )
end

function TestTypeLookup:TestLookupInvalidType()
   local t1 = ubx.type_get(NI, "__fooobarg__")
   luaunit.assert_nil(t1, "invalid type lookup succeded")
end

function TestTypeLookup:TestLookupInvalidHash()
   local hash = ffi.new("char["..tostring(ffi.C.TYPE_HASH_LEN+1)..']')
   local t1 = ubx.type_get_by_hash(NI, hash)
   luaunit.assert_nil(t1, "invalid type lookup succeded")
end

function TestTypeLookup:TestLookupInvalidHashstr()
   local hashstr = ffi.new("char["..tostring(ffi.C.TYPE_HASHSTR_LEN+1)..']')
   for i=0,ffi.C.TYPE_HASHSTR_LEN-1 do hashstr[i] = string.byte('a') end
   local t1 = ubx.type_get_by_hashstr(NI, hashstr)
   luaunit.assert_nil(t1, "invalid type lookup succeded")
end


os.exit( luaunit.LuaUnit.run() )
