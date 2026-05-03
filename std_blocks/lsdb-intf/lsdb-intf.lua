local ubx   = require("ubx")
local ffi   = require("ffi")
local utils = require("utils")
local bd    = require("blockdiagram")
local fmt   = string.format

local _lsdb_ok, lsdb = pcall(require, "lsdbus")
local _err_ok,  err  = pcall(require, "lsdbus.error")

local _missing_deps = {}
if not _lsdb_ok then _missing_deps[#_missing_deps+1] = "lsdbus" end
if not _err_ok  then _missing_deps[#_missing_deps+1] = "lsdbus.error" end

local BUS_RUN_TIMEOUT_USEC = 200000

local SERVICE =	"org.ubx.%s"

local _prefix = ubx.get_prefix()
local PLUGIN_DIR = _prefix .. "/share/ubx/lsdb-intf.d"

-- helpers
local function check_block(vt, name)
   local b = ubx.block_get(vt.nd, name)
   if b == nil then
      lsdb.throw(err.INVALID_ARGS, "invalid block '%s'", name)
   end
   return b
end

local function check_config(vt, bname, cname)
   local b = check_block(vt, bname)
   local c = ubx.block_config_get(b, cname)
   if c == nil then
      lsdb.throw(err.INVALID_ARGS, "invalid config '%s' for block '%s'", cname, bname)
   end
   return c, b
end

--- Methods
local function load_module(vt, module)
   vt.nd:load_module(module)
   vt:emitPropertiesChanged("CBlockTypes", "IBlockTypes")
end

-- a list of port_clone_conn ports
-- these are cleared in remove_block when blocks are removed
local wpccs = {}
local rppcs = {}

-- plugin registry: module_name → { vt=vtable, mod=module_table }
local plugins_reg = {}

-- context passed to plugin setup(); populated in start() once vt exists
local ctx

-- bus connection and org.ubx.node vtable; set in start(), used by step/stop and plugins
local bus, vt

local function create_block(vt,	type, name, conf)
   ubx.block_create(vt.nd, type, name, conf)
end

local function remove_block(vt, name)
   wpccs[name] = nil
   rppcs[name] = nil
   ubx.block_unload(vt.nd, name)
end

local function switch_state(vt, name, state)
   local b = ubx.block_get(vt.nd, name)
   ubx.block_tostate(b, state)
end

local function trigger_blocks(vt, blocks)
   for _,name in ipairs(blocks) do
      local b = ubx.block_get(vt.nd, name)
      b:do_step()
   end
end

local function connect(vt, srcbn, srcpn, tgtbn, tgtpn, ibtype, ibconfig)
   srcbn = srcbn ~= "" and srcbn or nil
   srcpn = srcpn ~= "" and srcpn or nil
   tgtbn = tgtbn ~= "" and tgtbn or nil
   tgtpn = tgtpn ~= "" and tgtpn or nil
   ibtype = ibtype ~= "" and ibtype or nil
   if ibconfig == "" or ibconfig == false then ibconfig = nil end

   assert(ubx.connect(vt.nd, srcbn, srcpn, tgtbn, tgtpn, ibtype, ibconfig))
   vt:emitPropertiesChanged("Connections")
end

local function set_config(vt, block, config, value)
   local function resolve_blkref(v, t, k)
      if type(v) ~= 'string' then return end
      local bn = string.match(v, ".*#([%w_%-%/]+)")
      if bn then
	 local ptr = ubx.block_get(vt.nd, bn)
	 if ptr then
	    t[k] = ptr
	 else
	    error("invalid block reference: "..v)
	 end
      end
   end

   if type(value) == 'table' then
      utils.maptree(resolve_blkref, value)
   end

   local c, b = check_config(vt, block, config)

   if b:get_block_state() == 'active' then
      lsdb.throw(err.FAILED, "changing active block config not allowed")
   end

   ubx.data_resize(c.value, 1)
   ubx.config_set(c, value)
end

local function get_config(vt, block, config)
   local c = check_config(vt, block, config)
   return lsdb.tovariant(c:tolua())
end

local function load_usc_json(vt, str)
   local sys = bd.load_str(str, 'json')
   sys:launch({ nd = vt.nd })
end

local function load_usc_lua(vt, str)
   local sys = bd.load_str(str, 'lua')
   sys:launch({ nd = vt.nd })
end

-- Return true if name matches any entry in the keeplist.
-- Entries anchored with ^ or $ are treated as Lua match patterns,
-- plain entries are compared as exact block instance names.
local function keeplist_match(name, keeplist)
   for _, entry in ipairs(keeplist) do
      if string.sub(entry, 1, 1) == '^' or string.sub(entry, -1) == '$' then
	 if string.match(name, entry) then return true end
      else
	 if name == entry then return true end
      end
   end
   return false
end

-- like ubx_node_clear, but with filters
local function clear_node(vt, keeplist)
   keeplist = keeplist or {}

   local function filter(b)
      if not ubx.is_instance(b) then return false end
      local name = b:get_name()
      if name == vt.blkname then return false end
      if keeplist_match(name, keeplist) then return false end
      return true
   end

   ubx.blocks_map(vt.nd, function(b) ubx.block_tostate(b, 'inactive') end, filter)
   ubx.blocks_map(vt.nd, function(b) ubx.block_tostate(b, 'preinit') end, filter)
   ubx.blocks_map(vt.nd, function(b) remove_block(vt, b:get_name()) end, filter)
end

local function write(vt, bn, pn, val)
   local pcc

   -- ensure the blocks (still) exists
   local b = ubx.block_get(vt.nd, bn)

   if not b then
      if wpccs[bn] then wpccs[bn][pn] = nil end
      lsdb.throw(err.INVALID_ARGS, "write: invalid block '%s'", bn)
   end

   if wpccs[bn] and wpccs[bn][pn] then
      pcc = wpccs[bn][pn]
   else
      pcc = ubx.port_clone_conn(b, pn, nil, nil, -1)
      wpccs[bn] = wpccs[bn] or {}
      wpccs[bn][pn] = pcc
   end

   ubx.port_write(pcc, val)
end

local function read(vt, bn, pn)
   local pcc

   local b = ubx.block_get(vt.nd, bn)

   if not b then
      if rppcs[bn] then rppcs[bn][pn] = nil end
      lsdb.throw(err.INVALID_ARGS, "read: invalid block '%s'", bn)
   end

   if rppcs[bn] and rppcs[bn][pn] then
      pcc = rppcs[bn][pn]
   else
      pcc = ubx.port_clone_conn(b, pn, nil, nil, -1)
      rppcs[bn] = rppcs[bn] or {}
      rppcs[bn][pn] = pcc
   end

   local ret, res = ubx.port_read(pcc)
   if ret < 0 then
      ubx.err("read failed: %d", ret);
      lsdb.throw(err.FAILED, "read from %s.%s failed: '%s'", bn, pn, ret)
   elseif ret == 0 then
      return lsdb.tovariant(false)
   end
   return lsdb.tovariant(res:tolua())
end

-- Property getters/setters

local function get_node_name(vt)
   return vt.nd:get_name()
end

local function get_cblock_types (vt)
   return ubx.blocks_map(
      vt.nd,
      function (b) return ubx.safe_tostr(b.name) end,
      ubx.is_cblock_proto)
end

local function get_iblock_types (vt)
   return ubx.blocks_map(
      vt.nd,
      function (b) return ubx.safe_tostr(b.name) end,
      ubx.is_iblock_proto)
end

-- get all cblocks including prototype and state
local function get_cblocks (vt)
   local function get_cblock_info(b)
      return {
	 b:get_name(),
	 b:get_prototype(),
	 b:get_block_state(),
      }
   end
   return ubx.blocks_map(vt.nd, get_cblock_info, ubx.is_cblock_instance)
end

-- get_block_info - get detailed information on a block
local function get_block_info(vt, name)
   local b = ubx.block_get(vt.nd, name)
   return lsdb.tovariant2(ubx.block_totab(b))
end

-- get_modules
local function get_modules(vt)
   local function get_mod_info(m)
      return {
	 ubx.safe_tostr(m.id),
	 ubx.safe_tostr(m.spdx_license_id),
      }
   end
   return ubx.modules_map(vt.nd, get_mod_info)
end

-- get_connections
local function get_connections(vt)
   local blocks = ubx.blocks_map(vt.nd, ubx.block_totab, ubx.is_instance)

   local r = {}
   for _,b in ipairs(blocks) do
      for _,p in ipairs(b.ports) do
	 for _,ib_out in ipairs(p.connections.outgoing) do
	    r[#r+1] = { from={ b.name, p.name }, to=ib_out }
	 end
	 for _,ib_in in ipairs(p.connections.incoming) do
	    r[#r+1] = { from=ib_in, to={ b.name, p.name } }
	 end
      end
   end
   return lsdb.tovariant2(r)
end

-- build the context table passed to plugin setup(); must be called after vt is set
local function make_ctx()
   ctx = {
      nd  = vt.nd,
      bus = bus,
      api = {
	 load_module  = function(...) return load_module(vt, ...) end,
	 create_block = function(...) return create_block(vt, ...) end,
	 remove_block = function(...) return remove_block(vt, ...) end,
	 switch_state = function(...) return switch_state(vt, ...) end,
	 trigger      = function(...) return trigger_blocks(vt, ...) end,
	 connect      = function(...) return connect(vt, ...) end,
	 set_config   = function(...) return set_config(vt, ...) end,
	 get_config   = function(...) return get_config(vt, ...) end,
	 write        = function(...) return write(vt, ...) end,
	 read         = function(...) return read(vt, ...) end,
	 load_usc_lua = function(...) return load_usc_lua(vt, ...) end,
	 load_usc_json= function(...) return load_usc_json(vt, ...) end,
	 clear_node   = function(...) return clear_node(vt, ...) end,
      }
   }
end

-- For short names (no leading '/'), strip .lua suffix to get the registry key
-- and resolve the file as <PLUGIN_DIR>/<key>.lua.
-- Absolute paths are used as-is for both key and file.
local function normalize_plugin_name(name)
   if name:sub(1,1) == '/' then return name, name end
   local key = name:gsub("%.lua$", "")
   return key, PLUGIN_DIR .. "/" .. key .. ".lua"
end

local function do_load_plugin(name)
   local key, path = normalize_plugin_name(name)
   if plugins_reg[key] then
      lsdb.throw(err.INVALID_ARGS, "plugin '%s' already loaded", key)
   end
   local chunk, loaderr = loadfile(path)
   if not chunk then
      lsdb.throw(err.FAILED, "failed to load plugin '%s': %s", path, loaderr)
   end
   local mod = chunk()
   if type(mod.init) ~= 'function' then
      lsdb.throw(err.FAILED, "plugin '%s' missing init() function", path)
   end
   local spec = mod.init(ctx)
   if type(spec) ~= 'table' or type(spec.path) ~= 'string' or type(spec.intf) ~= 'table' then
      lsdb.throw(err.FAILED, "plugin '%s' init() must return { path=string, intf=table }", path)
   end
   local pvt = lsdb.server.new(bus, spec.path, spec.intf)
   plugins_reg[key] = { vt=pvt, mod=mod }
end

local function do_unload_plugin(name)
   local key = normalize_plugin_name(name)
   local p = plugins_reg[key]
   if not p then
      lsdb.throw(err.INVALID_ARGS, "plugin '%s' not loaded", key)
   end
   if p.mod and type(p.mod.cleanup) == 'function' then p.mod.cleanup() end
   p.vt:unref()
   plugins_reg[key] = nil
end

local function load_plugin(vt, name) do_load_plugin(name) end

local function unload_plugin(vt, name)
   local key = normalize_plugin_name(name)
   if plugins_reg[key] and plugins_reg[key].builtin then
      lsdb.throw(err.INVALID_ARGS, "cannot unload built-in plugin '%s'", key)
   end
   do_unload_plugin(name)
end

local function list_plugins(vt)
   local names = {}
   for name in pairs(plugins_reg) do names[#names+1] = name end
   table.sort(names)
   return names
end

local intf = {
   name = "org.ubx.node",
   methods = {
      LoadModule = {
	 { direction='in', name='name', type='s' },
	 handler = load_module
      },

      CreateBlock = {
	 { direction='in', name='type', type='s' },
	 { direction='in', name='name', type='s' },
	 { direction='in', name='config', type='a{sv}' },
	 handler = create_block,
      },

      RemoveBlock = {
	 { direction='in', name='name', type='s' },
	 handler = remove_block,
      },

      SwitchState = {
	 { direction='in', name='name', type='s' },
	 { direction='in', name='state', type='s' },
	 handler = switch_state,
      },

      Trigger = {
	 { direction='in', name='blocks', type='as' },
	 handler = trigger_blocks,
      },

      GetBlockInfo = {
	 { direction='in', name='name', type='s' },
	 { direction='out', name='info', type='a{sv}' },
	 handler = get_block_info
      },

      Connect = {
	 { direction='in', name='srcbn', type='s' },
	 { direction='in', name='srcpn', type='s' },
	 { direction='in', name='tgtbn', type='s' },
	 { direction='in', name='tgtpn', type='s' },
	 { direction='in', name='ibtype', type='s' },
	 { direction='in', name='ibconfig', type='v' },
	 handler = connect,
      },

      SetConfig = {
	 { direction='in', name='block', type='s' },
	 { direction='in', name='config', type='s' },
	 { direction='in', name='value', type='v' },
	 handler = set_config,
      },

      GetConfig = {
	 { direction='in', name='block', type='s' },
	 { direction='in', name='config', type='s' },
	 { direction='out', name='value', type='v' },
	 handler = get_config,
      },

      Write = {
	 { direction='in', name='tgtbn', type='s' },
	 { direction='in', name='tgtpn', type='s' },
	 { direction='in', name='value', type='v' },
	 handler = write,
      },

      Read = {
	 { direction='in', name='tgtbn', type='s' },
	 { direction='in', name='tgtpn', type='s' },
	 { direction='out', name='value', type='v' },
	 handler = read,
      },

      LoadUSCJSON = {
	 { direction='in', name='usc', type='s' },
	 handler = load_usc_json,
      },

      LoadUSCLua = {
	 { direction='in', name='usc', type='s' },
	 handler = load_usc_lua,
      },

      ClearNode = {
	 { direction='in', name='keeplist', type='as' },
	 handler = clear_node,
      },
   },

   properties = {
      Node =        { access='read', type='s',      get=get_node_name },
      CBlockTypes = { access='read', type='as',     get=get_cblock_types },
      IBlockTypes = { access='read', type='as',     get=get_iblock_types },
      CBlocks =     { access='read', type='a(sss)', get=get_cblocks },
      Modules =     { access='read', type='a(ss)',  get=get_modules },
      Connections = { access='read', type='av',     get=get_connections },
   }
}

local pluginmgr_intf = {
   name = "org.ubx.pluginmanager",
   methods = {
      LoadPlugin = {
	 { direction='in', name='name', type='s' },
	 handler = load_plugin,
      },
      UnloadPlugin = {
	 { direction='in', name='name', type='s' },
	 handler = unload_plugin,
      },
      ListPlugins = {
	 { direction='out', name='plugins', type='as' },
	 handler = list_plugins,
      },
   },
}

function init(block)
   block = ffi.cast("ubx_block_t*", block)
   if #_missing_deps > 0 then
      ubx.err(block.nd, "lsdb-intf", "missing dependencies: " .. table.concat(_missing_deps, ", "))
      return false
   end
   block:config_add("plugins", "semicolon-separated list of plugin module names to load at startup", "char")
   return true
end

function start(block)
   block = ffi.cast("ubx_block_t*", block)
   local nd = block.nd
   local ndname = ubx.safe_tostr(nd.name)
   ubx.ffi_load_types(nd)

   bus = lsdb.open()
   bus:request_name(fmt(SERVICE, ndname))

   vt = lsdb.server.new(bus, "/", intf)
   vt.nd = nd
   vt.blkname = ubx.safe_tostr(block.name)
   vt:emitAllPropertiesChanged()
   plugins_reg["org.ubx.node"] = { vt=vt, builtin=true }

   local vt_pm = lsdb.server.new(bus, "/", pluginmgr_intf)
   plugins_reg["org.ubx.pluginmanager"] = { vt=vt_pm, builtin=true }

   make_ctx()

   local c = ubx.block_config_get(block, "plugins")
   if c then
      local plugin_str = c:tolua()
      if type(plugin_str) == 'string' and plugin_str ~= "" then
	 for name in string.gmatch(plugin_str, "[^;]+") do
	    do_load_plugin(name)
	 end
      end
   end

   return true
end

function step(block)
   while bus:run(BUS_RUN_TIMEOUT_USEC) > 0 do end
end

function stop(block)
   local names = {}
   for name in pairs(plugins_reg) do names[#names+1] = name end
   for _, name in ipairs(names) do do_unload_plugin(name) end
end

function cleanup(block)
end
