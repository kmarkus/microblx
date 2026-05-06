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

ubx.color = false

local assert_not_nil = luaunit.assert_not_nil
local assert_equals = luaunit.assert_equals
local assert_true = luaunit.assert_true
local assert_error_msg_contains = luaunit.assert_error_msg_contains

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

---
--- LoadUSCLua
---

function TestLsdbIntf:test_load_usc_lua()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_load_usc")

   _proxy('LoadUSCLua', make_thres_usc("t1", 5.0))
   assert_true(cblocks_contains(_proxy.CBlocks, "t1"))

   assert_equals(write_trigger_read(_proxy, "t1", 3.0), 0)
   assert_equals(write_trigger_read(_proxy, "t1", 7.0), 1)
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
]]))

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

   -- short name without .lua extension; key in registry must also be extension-free
   _pm_proxy('LoadPlugin', "lsdb_intf_test_plugin")

   local plugins = _pm_proxy('ListPlugins')
   assert_true(list_contains(plugins, "lsdb_intf_test_plugin"),
               "plugin loaded without .lua should be listed without extension")
end

function TestLsdbIntf:test_load_plugin_from_stddir_lua_suffix()
   _nd, _lsdb_blk, _bus, _proxy, _pm_proxy = create_node("test_load_plugin_stddir_lua")

   _pm_proxy('LoadPlugin', "lsdb_intf_test_plugin.lua")

   local plugins = _pm_proxy('ListPlugins')
   assert_true(list_contains(plugins, "lsdb_intf_test_plugin.lua"),
               "plugin loaded with .lua suffix should be listed with extension")
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

if not _RUNNER then os.exit(luaunit.LuaUnit.run()) end
