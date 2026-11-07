--
-- Test that a node name can only be used by one running node
--

local lu = require("luaunit")
local ubx = require("ubx")
local ffi = require("ffi")

local assert_equals = lu.assert_equals
local LOGLEVEL = ffi.C.UBX_LOGLEVEL_CRIT

TestNodeName = {}

local function create(name) return ubx.node_create(name, { loglevel = LOGLEVEL }) end

function TestNodeName:TestTaken()
   local nd = create("TestNodeNameTaken")
   lu.assert_false(pcall(create, "TestNodeNameTaken"))

   -- a name freed by node_rm can be used again
   ubx.node_rm(nd)
   ubx.node_rm(create("TestNodeNameTaken"))
end

--- as does the garbage collector
function TestNodeName:TestCollected()
   create("TestNodeNameGc")
   collectgarbage()
   collectgarbage()
   ubx.node_rm(create("TestNodeNameGc"))
end

function TestNodeName:TestOtherName()
   local nd1 = create("TestNodeNameA")
   local nd2 = create("TestNodeNameB")
   ubx.node_rm(nd1)
   ubx.node_rm(nd2)
end

--- also taken for another process
function TestNodeName:TestOtherProcess()
   local nd = create("TestNodeNameProc")
   local cmd = [[luajit -e 'local ok = pcall(require("ubx").node_create, "]]
      .. [[TestNodeNameProc", { loglevel = 2 }); print(ok)' 2>/dev/null]]
   local p = io.popen(cmd)
   local res = p:read("*l")
   p:close()
   assert_equals(res, "false")
   ubx.node_rm(nd)
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
