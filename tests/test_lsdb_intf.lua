local luaunit = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")
local lbutil = require("ubx/luablock-util")

local lsdb_available, lsdb = pcall(require, "lsdbus")

-- resolve the directory containing this test file so we can reference
-- the test plugin by absolute path regardless of working directory
local _src = debug.getinfo(1, 'S').source:match('@(.*)')
local _dir = _src and (_src:match('(.+)/[^/]+$') or '.') or 'tests'
if _dir:sub(1,1) ~= '/' then
   local cwd = io.popen("pwd"):read("*l")
   _dir = cwd .. "/" .. _dir
end
local TEST_DIR = _dir
local TEST_PLUGIN = TEST_DIR .. "/lsdb_intf_test_plugin.lua"
local INOUT_BLOCK = TEST_DIR .. "/lsdb_intf_inout_block.lua"


local assert_not_nil = luaunit.assert_not_nil
local assert_equals = luaunit.assert_equals
local assert_true = luaunit.assert_true
local assert_error_msg_contains = luaunit.assert_error_msg_contains
local assert_str_contains = luaunit.assert_str_contains

local fmt = string.format
local UBX_SRV  = 'org.ubx.%s'
local UBX_PATH = '/'
local UBX_INTF = 'org.ubx.node'
local UBX_PM_INTF = 'org.ubx.pluginmanager'

local THRES_USC = [[
return bd.system {
   imports = { "stdtypes", "lfrb", "threshold" },
   blocks = {
      { name = "%s", type = "ubx/threshold" },
   },
   configurations = {
      { name = "%s", config = { threshold = %s } },
   },
}
]]

local function make_thres_usc(name, threshold)
   return fmt(THRES_USC, name, name, tostring(threshold))
end

-- USC instantiating a luablock with a single in-out port (see
-- lsdb_intf_inout_block.lua). %s is replaced by the block name twice.
local INOUT_USC = [[
return bd.system {
   imports = { "stdtypes", "luablock" },
   blocks = {
      { name = "%s", type = "ubx/luablock" },
   },
   configurations = {
      { name = "%s", config = { lua_file = "]] .. INOUT_BLOCK .. [[" } },
   },
}
]]

local function make_inout_usc(name)
   return fmt(INOUT_USC, name, name)
end

--- return the port table named `pname` from a GetBlockInfo result, or nil
local function find_port(info, pname)
   for _, p in ipairs(info.ports or {}) do
      if p.name == pname then return p end
   end
   return nil
end

--- write a value to the threshold's "in" port, trigger and
--- read back the "state" port via the D-Bus interface.
--- The initial Read primes the read port clone connection
--- so that the output produced by Trigger is captured.
local function write_trigger_read(proxy, blkname, inval)
   proxy('Read', blkname, "state")
   proxy('Write', blkname, "in", lsdb.tovariant(inval))
   proxy('Trigger', { blkname })
   return proxy('Read', blkname, "state")
end

--- return true if val is found in list
local function list_contains(list, val)
   for _, v in ipairs(list) do
      if v == val then return true end
   end
   return false
end

--- return true if list contains a sub-table whose first element equals name
local function cblocks_contains(list, name)
   for _, entry in ipairs(list) do
      if entry[1] == name then return true end
   end
   return false
end

TestLsdbIntf = {}

local _nd
local _lsdb_blk
local _bus
local _proxy
local _pm_proxy

function TestLsdbIntf:setUp()
   if not lsdb_available then
      luaunit.skip("lsdbus not available")
   end
end

function TestLsdbIntf:tearDown()
   if _lsdb_blk then
      ubx.block_stop(_lsdb_blk)
      ubx.block_cleanup(_lsdb_blk)
   end
   if _nd then ubx.node_rm(_nd) end
   _nd = nil
   _lsdb_blk = nil
   _bus = nil
   _proxy = nil
   _pm_proxy = nil
end

local function create_node(ndname)
   local nd = ubx.node_create(ndname, { loglevel=7 })
   ubx.load_module(nd, "stdtypes")
   ubx.load_module(nd, "lfrb")
   ubx.load_module(nd, "threshold")

   local blk = lbutil.create(nd, "lsdb-intf", "lsdb0", "active", { period=100 })
   assert_not_nil(blk)

   local bus = lsdb.open('default')
   local proxy    = lsdb.proxy.new(bus, fmt(UBX_SRV, ndname), UBX_PATH, UBX_INTF)
   local pm_proxy = lsdb.proxy.new(bus, fmt(UBX_SRV, ndname), UBX_PATH, UBX_PM_INTF)
   return nd, blk, bus, proxy, pm_proxy
end

---
--- Properties
---

function TestLsdbIntf:test_property_node()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_prop_node")
   assert_equals(_proxy.Node, "test_prop_node")
end

function TestLsdbIntf:test_property_modules()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_prop_mods")
   local mods = _proxy.Modules
   assert_true(#mods > 0, "Modules should not be empty")
end

function TestLsdbIntf:test_property_blocktypes()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_prop_btypes")

   local cbt = _proxy.CBlockTypes
   assert_true(list_contains(cbt, "ubx/threshold"),
               "CBlockTypes should contain ubx/threshold")

   local ibt = _proxy.IBlockTypes
   assert_true(list_contains(ibt, "ubx/lfrb"),
               "IBlockTypes should contain ubx/lfrb")
end

function TestLsdbIntf:test_property_cblocks()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_prop_cblocks")
   local cbs = _proxy.CBlocks
   assert_true(cblocks_contains(cbs, "lsdb0"),
               "CBlocks should contain lsdb0")
end

---
--- LoadModule / CreateBlock / SwitchState / RemoveBlock
---

function TestLsdbIntf:test_create_and_remove_block()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_create_rm")

   _proxy('CreateBlock', "ubx/threshold", "t1", {})
   assert_true(cblocks_contains(_proxy.CBlocks, "t1"))

   _proxy('SetConfig', "t1", "threshold", lsdb.tovariant(5.0))
   _proxy('SwitchState', "t1", "inactive")
   _proxy('SwitchState', "t1", "preinit")
   _proxy('RemoveBlock', "t1")

   assert_true(not cblocks_contains(_proxy.CBlocks, "t1"))
end

function TestLsdbIntf:test_load_module()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_loadmod")

   local cbt_before = _proxy.CBlockTypes
   assert_true(not list_contains(cbt_before, "ubx/ramp_uint32"),
               "ramp_uint32 should not be loaded yet")

   _proxy('LoadModule', "ramp_uint32")

   local cbt_after = _proxy.CBlockTypes
   assert_true(list_contains(cbt_after, "ubx/ramp_uint32"),
               "ramp_uint32 should be loaded")
end

---
--- SetConfig / GetConfig
---

function TestLsdbIntf:test_set_get_config()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_cfg")

   _proxy('CreateBlock', "ubx/threshold", "t1", {})
   _proxy('SetConfig', "t1", "threshold", lsdb.tovariant(42.0))

   local val = _proxy('GetConfig', "t1", "threshold")
   assert_equals(val, 42.0)
end

---
--- GetBlockInfo
---

function TestLsdbIntf:test_get_block_info()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_blkinfo")

   local sys = bd.load_str(make_thres_usc("t1", 5.0), 'lua')
   sys:launch{ nd=_nd }

   local info = _proxy('GetBlockInfo', "t1")
   assert_not_nil(info)
   assert_equals(info.name, "t1")
   assert_equals(info.prototype, "ubx/threshold")
end

--- in-out ports must be reported with both in and out type info
function TestLsdbIntf:test_get_block_info_inout_port()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_blkinfo_inout")

   local sys = bd.load_str(make_inout_usc("io1"), 'lua')
   sys:launch{ nd=_nd }

   local info = _proxy('GetBlockInfo', "io1")
   assert_not_nil(info)

   local p = find_port(info, "io")
   assert_not_nil(p, "block should expose an 'io' port")

   -- a port carrying both directions is an in-out port
   assert_equals(p.in_type_name, "int32_t")
   assert_equals(p.out_type_name, "int32_t")
end

---
--- write-read convenience (the RPC sequence ubx-dbus --write-read performs)
---

function TestLsdbIntf:test_write_read()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_wr")

   -- io1 echoes its in-out port "io" on step (see lsdb_intf_inout_block.lua)
   local sys = bd.load_str(make_inout_usc("io1"), 'lua')
   sys:launch{ nd=_nd }

   -- mirror ubx-dbus --write-read: prime read, write, optionally step, read
   local function write_read(blk, port, val, step)
      _proxy('Read', blk, port)
      _proxy('Write', blk, port, lsdb.tovariant(val))
      if step then _proxy('Trigger', { blk }) end
      return _proxy('Read', blk, port)
   end

   -- with step the echoed value is read back from the same port
   assert_equals(write_read("io1", "io", 42, true), 42)
   -- without stepping there is no result yet on the out side
   assert_equals(write_read("io1", "io", 7, false), false)
end

---
--- LoadUSCLua
---

function TestLsdbIntf:test_load_usc_lua()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_load_usc")

   _proxy('LoadUSCLua', make_thres_usc("t1", 5.0), {})
   assert_true(cblocks_contains(_proxy.CBlocks, "t1"))

   assert_equals(write_trigger_read(_proxy, "t1", 3.0), 0)
   assert_equals(write_trigger_read(_proxy, "t1", 7.0), 1)
end

-- threshold model whose threshold is the parameter T
local THRES_PARAM_USC = [[
local T = bd.param("T", 5.0, "threshold")
return bd.system {
   imports = { "stdtypes", "lfrb", "threshold" },
   blocks = { { name = "t1", type = "ubx/threshold" } },
   configurations = { { name = "t1", config = { threshold = T } } },
}
]]

function TestLsdbIntf:test_load_usc_lua_param_default()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_load_usc_pdef")
   local warnings = _proxy('LoadUSCLua', THRES_PARAM_USC, {})
   assert_equals(warnings, {})
   assert_equals(_proxy('GetConfig', "t1", "threshold"), 5.0)
end

function TestLsdbIntf:test_load_usc_lua_param_value()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_load_usc_pval")
   _proxy('LoadUSCLua', THRES_PARAM_USC, { T="20" })
   assert_equals(_proxy('GetConfig', "t1", "threshold"), 20.0)
   assert_equals(write_trigger_read(_proxy, "t1", 7.0), 0)
   assert_equals(write_trigger_read(_proxy, "t1", 21.0), 1)
end

function TestLsdbIntf:test_load_usc_lua_param_unknown()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_load_usc_punk")
   assert_error_msg_contains("unknown parameter X; declared: T",
      function() _proxy('LoadUSCLua', THRES_PARAM_USC, { X="1" }) end)
   assert_true(not cblocks_contains(_proxy.CBlocks, "t1"))
   -- the context was closed: the next load works
   _proxy('LoadUSCLua', THRES_PARAM_USC, { T="3" })
   assert_equals(_proxy('GetConfig', "t1", "threshold"), 3.0)
end

function TestLsdbIntf:test_load_usc_lua_param_invalid()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_load_usc_pinv")
   assert_error_msg_contains('parameter T: invalid number "x"',
      function() _proxy('LoadUSCLua', THRES_PARAM_USC, { T="x" }) end)
   assert_error_msg_contains("invalid parameter name",
      function() _proxy('LoadUSCLua', THRES_PARAM_USC, { ["1T"]="1" }) end)
   assert_true(not cblocks_contains(_proxy.CBlocks, "t1"))
end

function TestLsdbIntf:test_load_usc_lua_param_conflict()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_load_usc_pconf")
   local usc = [[
bd.param("T", 1.0)
bd.param("T", 2.0)
return bd.system {}
]]
   assert_error_msg_contains("parameter T: default 2 in <string> conflicts with 1 in <string>",
      function() _proxy('LoadUSCLua', usc, {}) end)
   local warnings = _proxy('LoadUSCLua', usc, { T="3" })
   assert_equals(warnings, { "parameter T: default 2 in <string> conflicts with 1 in <string>" })

   -- a warning is no format string
   warnings = _proxy('LoadUSCLua', 'bd.param("F", "%d"); bd.param("F", "%f"); return bd.system {}',
		     { F="x" })
   assert_equals(warnings, { 'parameter F: default "%f" in <string> conflicts with "%d" in <string>' })
end

function TestLsdbIntf:test_handler_error_with_percent()
   -- the logged error message must not be used as format string
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_err_pct")
   assert_error_msg_contains("load 100% failed",
      function() _proxy('LoadUSCLua', 'error("load 100% failed")', {}) end)
end

function TestLsdbIntf:test_load_usc_lua_param_check()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_load_usc_pchk")
   local usc = [[
local T = bd.param("T", 5.0, "threshold", function(v) return v < 10, "must be < 10" end)
return bd.system {
   imports = { "stdtypes", "lfrb", "threshold" },
   blocks = { { name = "t1", type = "ubx/threshold" } },
   configurations = { { name = "t1", config = { threshold = T } } },
}
]]
   assert_error_msg_contains("parameter T: invalid value 20: must be < 10",
      function() _proxy('LoadUSCLua', usc, { T="20" }) end)
   assert_true(not cblocks_contains(_proxy.CBlocks, "t1"))
   _proxy('LoadUSCLua', usc, { T="8" })
   assert_equals(_proxy('GetConfig', "t1", "threshold"), 8.0)
end

---
--- ubx-dbus -D / --params
---

-- run ubx-dbus from the source tree, independent of PATH; on
-- installed systems (no source tree) fall back to the one in PATH
local UBX_DBUS = TEST_DIR .. "/../std_blocks/lsdb-intf/ubx-dbus"

do
   local f = io.open(UBX_DBUS)
   if f then f:close(); UBX_DBUS = "luajit " .. UBX_DBUS else UBX_DBUS = "ubx-dbus" end
end

--- run a shell command, return combined stdout+stderr and exit code.
--- a leading ubx-dbus runs the source tree version.
local function run(cmd)
   cmd = cmd:gsub("^ubx%-dbus", UBX_DBUS)
   local p = io.popen(cmd .. " 2>&1; echo \"rc=$?\"")
   local out = p:read("*a")
   p:close()
   local rc = tonumber(out:match("rc=(%d+)\n?$"))
   return out:gsub("rc=%d+\n?$", ""), rc
end

local function write_tmp(content, ext)
   -- os.tmpname creates the file, only use its unique name
   local base = os.tmpname()
   os.remove(base)
   local fn = base .. ext
   local f = assert(io.open(fn, "w"))
   f:write(content)
   f:close()
   return fn
end

function TestLsdbIntf:test_ubx_dbus_params_local()
   -- lists locally, no node and no bus needed
   local fn = write_tmp(THRES_PARAM_USC, ".usc")
   local out, rc = run("ubx-dbus --load-usc=" .. fn .. " --params")
   os.remove(fn)
   assert_equals(rc, 0)
   assert_equals((out:gsub("[ ]+\n", "\n")), " name  default  help\n T     5        threshold\n")
end

function TestLsdbIntf:test_ubx_dbus_params_required()
   local fn = write_tmp('local r = bd.param("R", tonumber, "r") + 1\nreturn bd.system{}', ".usc")
   local out, rc = run("ubx-dbus --load-usc=" .. fn .. " --params")
   os.remove(fn)
   assert_equals(rc, 1)
   assert_str_contains(out, " R     <required>  r")
   assert_str_contains(out, "error: listing incomplete: ")
end

function TestLsdbIntf:test_ubx_dbus_params_define()
   -- --params applies and checks -D like ubx-launch
   local fn = write_tmp(THRES_PARAM_USC, ".usc")
   local out, rc = run("ubx-dbus --load-usc=" .. fn .. " --params -D X=1")
   assert_equals(rc, 1)
   assert_equals(out, "error: unknown parameter X; declared: T\n")
   out, rc = run("ubx-dbus --load-usc=" .. fn .. " --params -D T=x")
   assert_equals(rc, 1)
   assert_str_contains(out, 'parameter T: invalid number "x"')
   out, rc = run("ubx-dbus --load-usc=" .. fn .. " --params -D T=7")
   os.remove(fn)
   assert_equals(rc, 0, out)
end

function TestLsdbIntf:test_ubx_dbus_params_load_error()
   local fn = write_tmp("error('bad model')", ".usc")
   local out, rc = run("ubx-dbus --load-usc=" .. fn .. " --params")
   os.remove(fn)
   assert_equals(rc, 1)
   assert_str_contains(out, "error: failed to load " .. fn)
   assert_str_contains(out, "bad model")
end

function TestLsdbIntf:test_ubx_dbus_define_requires_load_usc()
   for _,o in ipairs{ "-D T=1", "--params" } do
      local out, rc = run("ubx-dbus -n nonexistent " .. o)
      assert_equals(rc, 1)
      assert_equals(out, "-D and --params require --load-usc\n")
   end
end

function TestLsdbIntf:test_ubx_dbus_define_json()
   local fn = write_tmp("{}", ".json")
   local out, rc = run("ubx-dbus -n nonexistent --load-usc=" .. fn .. " -D T=1")
   os.remove(fn)
   assert_equals(rc, 1)
   assert_equals(out, "-D is not supported for json models\n")
end

function TestLsdbIntf:test_ubx_dbus_define_invalid()
   local fn = write_tmp(THRES_PARAM_USC, ".usc")
   local out, rc = run("ubx-dbus -n nonexistent --load-usc=" .. fn .. " -D T")
   os.remove(fn)
   assert_equals(rc, 1)
   assert_equals(out, "error: invalid define 'T', expected NAME=VALUE\n")
end

function TestLsdbIntf:test_ubx_dbus_load_usc_define()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_dbus_cli_def")
   local fn = write_tmp(THRES_PARAM_USC, ".usc")

   local out, rc = run("ubx-dbus -n test_dbus_cli_def --load-usc=" .. fn .. " -D X=1")
   assert_equals(rc, 3)
   assert_str_contains(out, "unknown parameter X; declared: T")

   out, rc = run("ubx-dbus -n test_dbus_cli_def --load-usc=" .. fn .. " -D T=42")
   os.remove(fn)
   assert_equals(rc, 0, out)
   assert_equals(_proxy('GetConfig', "t1", "threshold"), 42.0)
end

function TestLsdbIntf:test_ubx_dbus_load_usc_warning()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_dbus_cli_warn")
   local fn = write_tmp([[
bd.param("T", 1.0)
bd.param("T", 2.0)
return bd.system {}
]], ".usc")
   local out, rc = run("ubx-dbus -n test_dbus_cli_warn --load-usc=" .. fn .. " -D T=3")
   os.remove(fn)
   assert_equals(rc, 0, out)
   assert_equals(out, "warning: parameter T: default 2 in <string> conflicts with 1 in <string>\n")
end

---
--- ClearNode with keeplist
---

function TestLsdbIntf:test_clearnode_keeplist()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_keeplist")

   _proxy('LoadUSCLua', fmt([[
return bd.system {
   imports = { "stdtypes", "lfrb", "threshold" },
   blocks = {
      { name = "t1", type = "ubx/threshold" },
      { name = "t2", type = "ubx/threshold" },
   },
   configurations = {
      { name = "t1", config = { threshold = 5.0 } },
      { name = "t2", config = { threshold = 10.0 } },
   },
}
]]), {})

   assert_true(cblocks_contains(_proxy.CBlocks, "t1"))
   assert_true(cblocks_contains(_proxy.CBlocks, "t2"))

   _proxy('ClearNode', { "t2" })

   assert_true(not cblocks_contains(_proxy.CBlocks, "t1"))
   assert_true(cblocks_contains(_proxy.CBlocks, "t2"))
end

---
--- Write / Trigger / Read after ClearNode (regression)
---

function TestLsdbIntf:test_write_after_clearnode()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_lsdb_clearnode")

   -- first composition: threshold at 5.0
   local sys1 = bd.load_str(make_thres_usc("t1", 5.0), 'lua')
   sys1:launch{ nd=_nd }

   assert_equals(write_trigger_read(_proxy, "t1", 3.0), 0)
   assert_equals(write_trigger_read(_proxy, "t1", 7.0), 1)

   _proxy('ClearNode', {})

   -- second composition: threshold at 10.0
   local sys2 = bd.load_str(make_thres_usc("t2", 10.0), 'lua')
   sys2:launch{ nd=_nd }

   assert_equals(write_trigger_read(_proxy, "t2", 8.0), 0)
   assert_equals(write_trigger_read(_proxy, "t2", 12.0), 1)
end

---
--- Write / Trigger / Read after RemoveBlock (regression)
---

function TestLsdbIntf:test_write_after_remove_block()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_lsdb_removeblock")

   local usc = fmt([[
return bd.system {
   imports = { "stdtypes", "lfrb", "threshold" },
   blocks = {
      { name = "t1", type = "ubx/threshold" },
      { name = "t2", type = "ubx/threshold" },
   },
   configurations = {
      { name = "t1", config = { threshold = 5.0 } },
      { name = "t2", config = { threshold = 10.0 } },
   },
}
]])

   local sys = bd.load_str(usc, 'lua')
   sys:launch{ nd=_nd }

   assert_equals(write_trigger_read(_proxy, "t1", 3.0), 0)
   assert_equals(write_trigger_read(_proxy, "t1", 7.0), 1)
   assert_equals(write_trigger_read(_proxy, "t2", 8.0), 0)

   _proxy('SwitchState', "t1", "inactive")
   _proxy('SwitchState', "t1", "preinit")
   _proxy('RemoveBlock', "t1")

   assert_equals(write_trigger_read(_proxy, "t2", 12.0), 1)
   assert_equals(write_trigger_read(_proxy, "t2", 5.0), 0)
end

---
--- Error reporting and self-protection
---

function TestLsdbIntf:test_self_ops_rejected()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_self_ops")

   assert_error_msg_contains("not allowed on the lsdb-intf block",
      function() _proxy('SwitchState', "lsdb0", "inactive") end)
   assert_error_msg_contains("not allowed on the lsdb-intf block",
      function() _proxy('Trigger', { "lsdb0" }) end)
   assert_error_msg_contains("not allowed on the lsdb-intf block",
      function() _proxy('RemoveBlock', "lsdb0") end)
   assert_equals(_lsdb_blk:get_block_state(), "active")
end

function TestLsdbIntf:test_remove_active_triggee_fails()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_rm_triggee")

   ubx.load_module(_nd, "ptrig")
   local sys = bd.load_str([[
return bd.system {
   imports = { "stdtypes", "lfrb", "threshold", "ptrig" },
   blocks = {
      { name = "t1", type = "ubx/threshold" },
      { name = "pt", type = "ubx/ptrig" },
   },
   configurations = {
      { name = "t1", config = { threshold = 5.0 } },
      { name = "pt", config = { period = { sec=0, usec=1000 }, chain0 = { { b="#t1" } } } },
   },
}
]], 'lua')
   sys:launch{ nd=_nd }

   assert_error_msg_contains("is triggered by active block 'pt'",
      function() _proxy('RemoveBlock', "t1") end)
   assert_true(cblocks_contains(_proxy.CBlocks, "t1"))
end

function TestLsdbIntf:test_switch_state_errors()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_switch_errors")

   _proxy('CreateBlock', "ubx/threshold", "t1", {})
   assert_error_msg_contains("invalid state 'bogus'",
      function() _proxy('SwitchState', "t1", "bogus") end)

   -- threshold init fails without a threshold config
   assert_error_msg_contains("failed to switch block 't1' to state 'active'",
      function() _proxy('SwitchState', "t1", "active") end)
end

function TestLsdbIntf:test_trigger_inactive_fails()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_trigger_inactive")

   _proxy('CreateBlock', "ubx/threshold", "t1", {})
   assert_error_msg_contains("failed to trigger block 't1'",
      function() _proxy('Trigger', { "t1" }) end)
end

-- the kept block's port clones are connected via iblocks that
-- ClearNode removes; they must be recreated, not reused
function TestLsdbIntf:test_read_after_clearnode_keep()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_clearnode_keep")

   local sys = bd.load_str(make_thres_usc("t1", 5.0), 'lua')
   sys:launch{ nd=_nd }

   assert_equals(write_trigger_read(_proxy, "t1", 3.0), 0)
   _proxy('ClearNode', { "t1" })
   assert_equals(write_trigger_read(_proxy, "t1", 7.0), 1)
end

function TestLsdbIntf:test_clearnode_stops_kept_trigger()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_clearnode_trig")

   ubx.load_module(_nd, "ptrig")
   local sys = bd.load_str([[
return bd.system {
   imports = { "stdtypes", "lfrb", "threshold", "ptrig" },
   blocks = {
      { name = "t1", type = "ubx/threshold" },
      { name = "pt", type = "ubx/ptrig" },
   },
   configurations = {
      { name = "t1", config = { threshold = 5.0 } },
      { name = "pt", config = { period = { sec=0, usec=1000 }, chain0 = { { b="#t1" } } } },
   },
}
]], 'lua')
   sys:launch{ nd=_nd }
   assert_equals(ubx.block_get(_nd, "pt"):get_block_state(), "active")

   _proxy('ClearNode', { "pt" })

   assert_true(not cblocks_contains(_proxy.CBlocks, "t1"))
   assert_equals(ubx.block_get(_nd, "pt"):get_block_state(), "inactive")

   -- t1 was removed from the chain
   local info = _proxy('GetBlockInfo', "pt")
   for _, c in ipairs(info.configs) do
      if c.name == "chain0" then assert_equals(c.value, nil) end
   end
end

-- removing a block also removes the iblocks of its port clones
function TestLsdbIntf:test_remove_block_removes_pccs()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_rm_pccs")

   local sys = bd.load_str(make_thres_usc("t1", 5.0), 'lua')
   sys:launch{ nd=_nd }
   assert_equals(write_trigger_read(_proxy, "t1", 7.0), 1)

   local function num_pccs()
      local n = 0
      ubx.blocks_map(_nd, function(b)
	 if b:get_name():match("^PCC") then n = n + 1 end
      end)
      return n
   end
   assert_true(num_pccs() > 0)

   _proxy('SwitchState', "t1", "preinit")
   _proxy('RemoveBlock', "t1")
   assert_equals(num_pccs(), 0)
end

function TestLsdbIntf:test_connect_ibconfig_not_table()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_connect_ibconfig")

   local sys = bd.load_str(make_thres_usc("t1", 5.0), 'lua')
   sys:launch{ nd=_nd }
   assert_error_msg_contains("ibconfig must be a table",
      function() _proxy('Connect', "t1", "state", "", "", "ubx/lfrb", lsdb.tovariant("{}")) end)
end

---
--- Plugins: ListPlugins / LoadPlugin / UnloadPlugin
---

function TestLsdbIntf:test_list_plugins_builtins_present()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_plugins_builtins")
   local plugins = _pm_proxy('ListPlugins')
   assert_true(list_contains(plugins, "org.ubx.node"))
   assert_true(list_contains(plugins, "org.ubx.pluginmanager"))
   assert_true(not list_contains(plugins, TEST_PLUGIN))
end

function TestLsdbIntf:test_load_plugin()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_load_plugin")

   _pm_proxy('LoadPlugin', TEST_PLUGIN)

   local plugins = _pm_proxy('ListPlugins')
   assert_true(list_contains(plugins, TEST_PLUGIN), "loaded plugin should appear in ListPlugins")
end

function TestLsdbIntf:test_load_plugin_from_stddir()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_load_plugin_stddir")

   _pm_proxy('LoadPlugin', "lsdb_intf_test_plugin.lua")

   local plugins = _pm_proxy('ListPlugins')
   assert_true(list_contains(plugins, "lsdb_intf_test_plugin.lua"),
               "plugin should be listed under the exact name it was loaded with")
end

function TestLsdbIntf:test_load_unload_plugin()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_unload_plugin")

   _pm_proxy('LoadPlugin', TEST_PLUGIN)
   assert_true(list_contains(_pm_proxy('ListPlugins'), TEST_PLUGIN))

   _pm_proxy('UnloadPlugin', TEST_PLUGIN)
   assert_true(not list_contains(_pm_proxy('ListPlugins'), TEST_PLUGIN))

   -- reload should succeed after unload
   _pm_proxy('LoadPlugin', TEST_PLUGIN)
   assert_true(list_contains(_pm_proxy('ListPlugins'), TEST_PLUGIN))
end

function TestLsdbIntf:test_plugin_dbus_object()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_plugin_dbus")

   _pm_proxy('LoadPlugin', TEST_PLUGIN)

   local pp = lsdb.proxy.new(_bus,
                              fmt(UBX_SRV, "test_plugin_dbus"),
                              "/testplugin", "org.test.plugin")

   assert_equals(pp('Echo', "hello"), "hello")
   assert_equals(pp('NodeName'), "test_plugin_dbus")
end

function TestLsdbIntf:test_plugin_handler_error()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_plugin_err")

   _pm_proxy('LoadPlugin', TEST_PLUGIN)

   local pp = lsdb.proxy.new(_bus,
                              fmt(UBX_SRV, "test_plugin_err"),
                              "/testplugin", "org.test.plugin")
   assert_error_msg_contains("plugin handler failed", pp, 'Fail')
end

function TestLsdbIntf:test_plugin_load_usc_lua_number_param()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_plugin_usc_num")
   _pm_proxy('LoadPlugin', TEST_PLUGIN)
   local pp = lsdb.proxy.new(_bus, fmt(UBX_SRV, "test_plugin_usc_num"),
			     "/testplugin", "org.test.plugin")
   pp('LoadUSCParam', THRES_PARAM_USC, 12.5)
   assert_equals(_proxy('GetConfig', "t1", "threshold"), 12.5)
end

function TestLsdbIntf:test_plugin_ctx_api()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_plugin_ctx")

   _pm_proxy('LoadPlugin', TEST_PLUGIN)

   local pp = lsdb.proxy.new(_bus,
                              fmt(UBX_SRV, "test_plugin_ctx"),
                              "/testplugin", "org.test.plugin")
   assert_equals(pp('NodeName'), "test_plugin_ctx")
end

function TestLsdbIntf:test_unload_builtin_plugin_fails()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_unload_builtin")

   assert_error_msg_contains("cannot unload built-in plugin",
      function() _pm_proxy('UnloadPlugin', "org.ubx.node") end)

   assert_error_msg_contains("cannot unload built-in plugin",
      function() _pm_proxy('UnloadPlugin', "org.ubx.pluginmanager") end)
end

function TestLsdbIntf:test_load_plugin_notfound()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_plugin_notfound")

   assert_error_msg_contains("failed to load plugin",
      function() _pm_proxy('LoadPlugin', "/nonexistent/no_such_plugin.lua") end)
end

function TestLsdbIntf:test_load_plugin_duplicate()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_plugin_dup")

   _pm_proxy('LoadPlugin', TEST_PLUGIN)

   assert_error_msg_contains("already loaded",
      function() _pm_proxy('LoadPlugin', TEST_PLUGIN) end)
end

function TestLsdbIntf:test_plugin_startup_config()
   if not lsdb_available then luaunit.skip("lsdbus not available") end

   local ndname = "test_plugin_startup"
   _nd = ubx.node_create(ndname, { loglevel=7 })
   ubx.load_module(_nd, "stdtypes")
   ubx.load_module(_nd, "lfrb")
   ubx.load_module(_nd, "threshold")

   -- go to inactive first so that init() runs and the plugins config is created
   _lsdb_blk = lbutil.create(_nd, "lsdb-intf", "lsdb0", "inactive", { period=100 })
   assert_not_nil(_lsdb_blk)

   -- set the plugins config, then start — start() will load it
   local c = ubx.block_config_get(_lsdb_blk, "plugins")
   ubx.config_set(c, TEST_PLUGIN)
   ubx.block_tostate(_lsdb_blk, "active")

   _bus = lsdb.open('default')
   _proxy    = lsdb.proxy.new(_bus, fmt(UBX_SRV, ndname), UBX_PATH, UBX_INTF)
   _pm_proxy = lsdb.proxy.new(_bus, fmt(UBX_SRV, ndname), UBX_PATH, UBX_PM_INTF)

   assert_true(list_contains(_pm_proxy('ListPlugins'), TEST_PLUGIN),
               "plugin from startup config should appear in ListPlugins")
end

---
--- Plugin error handling: malformed / invalid plugins
---

TestLsdbIntfPluginErrors = {}

local function write_tmpfile(content)
   local path = os.tmpname() .. ".lua"
   local f = assert(io.open(path, 'w'))
   f:write(content)
   f:close()
   return path
end

function TestLsdbIntfPluginErrors:setUp()
   if not lsdb_available then luaunit.skip("lsdbus not available") end
end

local function create_pm_node(suffix)
   local nd, blk, bus, _, pm = create_node("tpe_" .. suffix)
   return nd, blk, bus, pm
end

function TestLsdbIntfPluginErrors:test_syntax_error()
   local nd, blk, bus, pm = create_pm_node("syntax")
   local path = write_tmpfile("this is not valid lua ][")
   -- error must name the file and include the actual Lua parse error
   assert_error_msg_contains(path,
      function() pm('LoadPlugin', path) end)
   assert_error_msg_contains("failed to load plugin",
      function() pm('LoadPlugin', path) end)
   ubx.block_stop(blk); ubx.block_cleanup(blk); ubx.node_rm(nd)
   os.remove(path)
end

function TestLsdbIntfPluginErrors:test_returns_nil()
   local nd, blk, bus, pm = create_pm_node("retnil")
   local path = write_tmpfile("return nil")
   assert_error_msg_contains("must return a table with an init() function",
      function() pm('LoadPlugin', path) end)
   ubx.block_stop(blk); ubx.block_cleanup(blk); ubx.node_rm(nd)
   os.remove(path)
end

function TestLsdbIntfPluginErrors:test_returns_non_table()
   local nd, blk, bus, pm = create_pm_node("retstr")
   local path = write_tmpfile('return "oops"')
   assert_error_msg_contains("must return a table with an init() function",
      function() pm('LoadPlugin', path) end)
   ubx.block_stop(blk); ubx.block_cleanup(blk); ubx.node_rm(nd)
   os.remove(path)
end

function TestLsdbIntfPluginErrors:test_missing_init()
   local nd, blk, bus, pm = create_pm_node("noinit")
   local path = write_tmpfile("return {}")
   assert_error_msg_contains("must return a table with an init() function",
      function() pm('LoadPlugin', path) end)
   ubx.block_stop(blk); ubx.block_cleanup(blk); ubx.node_rm(nd)
   os.remove(path)
end

function TestLsdbIntfPluginErrors:test_init_not_function()
   local nd, blk, bus, pm = create_pm_node("initnotfn")
   local path = write_tmpfile("return { init = 42 }")
   assert_error_msg_contains("must return a table with an init() function",
      function() pm('LoadPlugin', path) end)
   ubx.block_stop(blk); ubx.block_cleanup(blk); ubx.node_rm(nd)
   os.remove(path)
end

function TestLsdbIntfPluginErrors:test_init_returns_nil()
   local nd, blk, bus, pm = create_pm_node("initnil")
   local path = write_tmpfile("return { init = function() return nil end }")
   assert_error_msg_contains("init() must return { path=string, intf=table }",
      function() pm('LoadPlugin', path) end)
   ubx.block_stop(blk); ubx.block_cleanup(blk); ubx.node_rm(nd)
   os.remove(path)
end

function TestLsdbIntfPluginErrors:test_init_returns_non_table()
   local nd, blk, bus, pm = create_pm_node("initnontab")
   local path = write_tmpfile('return { init = function() return "bad" end }')
   assert_error_msg_contains("init() must return { path=string, intf=table }",
      function() pm('LoadPlugin', path) end)
   ubx.block_stop(blk); ubx.block_cleanup(blk); ubx.node_rm(nd)
   os.remove(path)
end

function TestLsdbIntfPluginErrors:test_init_missing_path()
   local nd, blk, bus, pm = create_pm_node("nopath")
   local path = write_tmpfile(
      "return { init = function() return { intf = {} } end }")
   assert_error_msg_contains("init() must return { path=string, intf=table }",
      function() pm('LoadPlugin', path) end)
   ubx.block_stop(blk); ubx.block_cleanup(blk); ubx.node_rm(nd)
   os.remove(path)
end

function TestLsdbIntfPluginErrors:test_init_missing_intf()
   local nd, blk, bus, pm = create_pm_node("nointf")
   local path = write_tmpfile(
      'return { init = function() return { path = "/x" } end }')
   assert_error_msg_contains("init() must return { path=string, intf=table }",
      function() pm('LoadPlugin', path) end)
   ubx.block_stop(blk); ubx.block_cleanup(blk); ubx.node_rm(nd)
   os.remove(path)
end

function TestLsdbIntfPluginErrors:test_init_path_not_string()
   local nd, blk, bus, pm = create_pm_node("badpath")
   local path = write_tmpfile(
      "return { init = function() return { path = 99, intf = {} } end }")
   assert_error_msg_contains("init() must return { path=string, intf=table }",
      function() pm('LoadPlugin', path) end)
   ubx.block_stop(blk); ubx.block_cleanup(blk); ubx.node_rm(nd)
   os.remove(path)
end

function TestLsdbIntfPluginErrors:test_init_intf_not_table()
   local nd, blk, bus, pm = create_pm_node("badintf")
   local path = write_tmpfile(
      'return { init = function() return { path = "/x", intf = "bad" } end }')
   assert_error_msg_contains("init() must return { path=string, intf=table }",
      function() pm('LoadPlugin', path) end)
   ubx.block_stop(blk); ubx.block_cleanup(blk); ubx.node_rm(nd)
   os.remove(path)
end

function TestLsdbIntfPluginErrors:test_cleanup_error_unload()
   local nd, blk, bus, pm = create_pm_node("cleanuperr")
   local path = write_tmpfile([[
return {
   init = function() return { path = "/cleanuperr", intf = { name = "org.test.cleanuperr",
          methods = { Ping = { handler = function() end } } } } end,
   cleanup = function() error("cleanup failed") end,
}]])
   pm('LoadPlugin', path)
   pm('UnloadPlugin', path)
   assert_true(not list_contains(pm('ListPlugins'), path))
   ubx.block_stop(blk); ubx.block_cleanup(blk); ubx.node_rm(nd)
   os.remove(path)
end

function TestLsdbIntfPluginErrors:test_register_error_cleanup()
   local nd, blk, bus, pm = create_pm_node("regerr")
   local marker = os.tmpname()
   local plugin = [[
return {
   init = function() return { path = "/regerr", intf = { name = "org.test.regerr",
          methods = { Ping = { handler = function() end } } } } end,
   cleanup = function() local f = io.open("%s", "w"); f:write("%s"); f:close() end,
}]]
   local path1 = write_tmpfile(fmt(plugin, marker, "1"))
   local path2 = write_tmpfile(fmt(plugin, marker, "2"))
   pm('LoadPlugin', path1)
   -- same object path and interface: registering fails
   assert_error_msg_contains("failed to add vtable",
      function() pm('LoadPlugin', path2) end)
   assert_equals(io.open(marker):read("*a"), "2")
   assert_true(not list_contains(pm('ListPlugins'), path2))
   ubx.block_stop(blk); ubx.block_cleanup(blk); ubx.node_rm(nd)
   os.remove(path1); os.remove(path2); os.remove(marker)
end

if not _RUNNER then os.exit(luaunit.LuaUnit.run()) end
