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

-- in a child process, to check and not print the stderr message
function TestNodeName:TestTaken()
   local cmd = [[luajit -e 'local ubx = require("ubx")
      local nd = ubx.node_create("TestNodeNameTaken", { loglevel = 2 })
      print(pcall(ubx.node_create, "TestNodeNameTaken", { loglevel = 2 }))' 2>&1]]
   local p = io.popen(cmd)
   local out = p:read("*a")
   p:close()
   lu.assert_str_contains(out, "a node named TestNodeNameTaken is already running")
   lu.assert_str_contains(out, "false")
end

-- a name freed by node_rm can be used again
function TestNodeName:TestReuse()
   ubx.node_rm(create("TestNodeNameReuse"))
   ubx.node_rm(create("TestNodeNameReuse"))
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

--- several nodes in a process can use one module: its types are static,
--- but each node has a copy
function TestNodeName:TestSharedModule()
   local nd1 = create("TestNodeNameMod1")
   local nd2 = create("TestNodeNameMod2")

   for _, m in ipairs { "stdtypes", "lfrb", "trig", "ramp" } do
      ubx.load_module(nd1, m)
      ubx.load_module(nd2, m)
   end

   lu.assert_false(ubx.type_get(nd1, "double") == ubx.type_get(nd2, "double"))
   assert_equals(ubx.type_get(nd2, "double").nd, nd2)

   ubx.node_rm(nd1)
   ubx.block_create(nd2, "ubx/ramp", "r1")	-- still intact
   ubx.node_rm(nd2)
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
