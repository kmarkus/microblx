--
-- blockdiagram: DSL for specifying and launching microblx system
-- compositions
--
-- Copyright (C) 2013 Markus Klotzbuecher <markus.klotzbuecher@mech.kuleuven.be>
-- Copyright (C) 2014-2020 Markus Klotzbuecher <mk@mkio.de>
--
-- SPDX-License-Identifier: BSD-3-Clause
--

local ubx = require "ubx"
local umf = require "umf"
local utils = require "utils"
local has_json, json = pcall(require, "cjson")
local strict = require "strict"

local M={}

-- shortcuts
local foreach = utils.foreach
local ts = tostring
local fmt = string.format
local insert = table.insert
local safets = ubx.safe_tostr

-- node configuration
_NC = nil
USE_STDERR = false

local red=ubx.red
local blue=ubx.blue
local cyan=ubx.cyan
local green=ubx.green
local yellow=ubx.yellow
local magenta=ubx.magenta

local crit, err, warn, notice, info = nil, nil, nil, nil, nil

local function stderr(msg)
   if USE_STDERR then utils.stderr(msg) end
end

--- Create logging helper functions.
local function def_loggers(ni, src)
   err = function(format, ...)
      local msg = fmt(format, unpack{...}); stderr(msg); ubx.err(ni, src, msg)
   end
   warn = function(format, ...)
      local msg = fmt(format, unpack{...}); stderr(msg); ubx.warn(ni, src, msg)
   end
   notice = function(format, ...)
      local msg = fmt(format, unpack{...}); stderr(msg); ubx.notice(ni, src, msg)
   end
   info = function(format, ...)
      local msg = fmt(format, unpack{...}); stderr(msg); ubx.info(ni, src, msg)
   end
end

local function err_exit(code, format, ...)
   err(format, ...)
   os.exit(code)
end

--- Return the block table identified by bfqn at the level of sys
-- @param sys system
-- @param bfqn block fqn string
local function blocktab_get(sys, bfqn)
   local s = sys

   -- first index to the right subsystem
   local elem = utils.split(bfqn, "%/")
   local bname = elem[#elem]

   for i=1,#elem-1 do
      s = s.subsystems[elem[i]]
      if not M.is_system(s) then return false end
   end

   -- then locate the right block
   for _,v in pairs(s.blocks) do
      if v.name==bname then return v end
   end
   return false
end

--- Check whether val is a nodeconfig reference
-- @param val value to check
-- @return nil or nodeconfig string name
local function check_noderef(val)
   if type(val) ~= 'string' then return end
   return string.match(val, "^%s*&([%w_-]+)")
end

--- apply func to systems including all subsystems
-- @param func function(system, name, parent-system)
-- @param root root system
-- @return list of return values of func
local function mapsys(func, root)
   local res = {}

   local function __mapsys(sys,name,psys)
      res[#res+1] = func(sys, name, psys)
      if not sys.subsystems then return end
      for k,s in pairs(sys.subsystems) do __mapsys(s, k, sys) end
   end
   __mapsys(root)
   return res
end

--- Apply func to all objs of system[systab] in breadth first
-- @param func function(obj, index/key, parent_system)
-- @param root_sys root system
-- @param systab which system table to map (blocks, configurations, ...)
-- @return list of return values of func
local function mapobj_bf(func, root_sys, systab)
   local res = {}
   local queue = { root_sys }

   -- breadth first
   while #queue > 0 do
      local next_sys = table.remove(queue, 1) -- pop

      -- process all nc's of s
      foreach(
	 function (o, k)
	    res[#res+1] = func(o, k, next_sys)
	 end, next_sys[systab])

      -- add s's subsystems to queue
      foreach(function (subsys) queue[#queue+1] = subsys end, next_sys.subsystems)
   end

   return res
end

local function mapblocks(func, root_sys) return mapobj_bf(func, root_sys, 'blocks') end
local function mapconns(func, root_sys) return mapobj_bf(func, root_sys, 'connections') end
local function mapconfigs(func, root_sys) return mapobj_bf(func, root_sys, 'configurations') end
local function mapndconfigs(func, root_sys) return mapobj_bf(func, root_sys, 'node_configurations') end
local function mapimports(func, root_sys) return mapobj_bf(func, root_sys, 'imports') end

---
--- Model
---
local AnySpec=umf.AnySpec
local NumberSpec=umf.NumberSpec
local StringSpec=umf.StringSpec
local TableSpec=umf.TableSpec
local ObjectSpec=umf.ObjectSpec

local system = umf.class("system")

--- imports spec
local imports_spec = TableSpec
{
   name='imports',
   sealed='both',
   array = { StringSpec{} },
}

-- blocks
local blocks_spec = TableSpec
{
   name='blocks',
   array = {
      TableSpec
      {
	 name='block',
	 dict = { name=StringSpec{}, type=StringSpec{} },
	 sealed='both',
      }
   },
   sealed='both',
}

-- connections
local function conn_check_srctgt(class, conn, vres)
   local res = true
   if not conn._src then
      umf.add_msg(vres, "err", "invalid src block ".. conn.src)
      res = false
   end
   if not conn._tgt then
      umf.add_msg(vres, "err", "invalid tgt block ".. conn.tgt)
      res = false
   end
   return res
end

local connections_spec = TableSpec
{
   name='connections',
   array = {
      TableSpec
      {
	 name='connection',
	 postcheck = conn_check_srctgt,
	 dict = {
	    src=StringSpec{},
	    tgt=StringSpec{},
	    buffer_length=NumberSpec{ min=0 },
	 },
	 sealed='both',
	 optional={'buffer_length' },
      }
   },
   sealed='both',
}


-- node_config
local node_config_spec = TableSpec {
   name='node configs',
   sealed='both',
   dict = {
      __other = {
	 TableSpec {
	    name = 'node config',
	    sealed='both',
	    dict = {
	       type = StringSpec{},
	       config = AnySpec{},
	    }
	 }
      }
   }
}

-- configuration
local function config_check_tgt(class, cfg, vres)
   if cfg._tgt then return true end
   umf.add_msg(vres, "err", "invalid target block ".. cfg.name)
   return false
end

local configs_spec = TableSpec
{
   name='configurations',
   -- regular block configs
   array = {
      TableSpec
      {
	 name='configuration',
	 postcheck = config_check_tgt,
	 dict = {
	    name=StringSpec{},
	    config=TableSpec{ name="config" },
	 },
	 sealed='both',
      },
   },
   sealed='both'
}

local subsystems_spec = TableSpec {
   name='subsystems',
   dict={}, -- self reference added below
   sealed='both',
}

-- Check that hash references are valid
local function sys_check_block_ref(class, sys, vres)
   local res = true

   local function check_config(c,_,s)
      local function check_hash(val, tab, key)
	 local name = string.match(val, ".*#([%w_%-%/]+)")
	 if not name then return end
	 local bt = blocktab_get(s, name)
	 if not bt then
	    umf.add_msg(vres, "err", "unable to resolve block ref "..val)
	    res = false
	 end
      end

      utils.maptree(check_hash,
		    c.config,
		    function(v,_) return type(v)=='string' end)
   end

   mapconfigs(check_config, sys)
   return res
end

--- Check all nodecfg references
local function sys_check_nodecfg_refs(class, sys, vres)
   local res = true

   -- first build a nodecfg table
   local nc_list = {}
   mapndconfigs(
      function (o,k) nc_list[k] = o end, sys)

   mapconfigs(
      function (c,k,p)
	 for name,val in pairs(c.config) do
	    local nodecfg = check_noderef(val)
	    if nodecfg and not nc_list[nodecfg] then
	       res = false
	       umf.add_msg(vres, "err",	"unable to resolve node config &"..nodecfg)
	    end
	 end
      end, sys)
   return res
end

local function sys_check_refs(class, sys, vres)
   local res1 = sys_check_nodecfg_refs(class, sys, vres)
   local res2 =	sys_check_block_ref(class, sys, vres)
   return res1 and res2
end

--- system spec
local system_spec = ObjectSpec
{
   name='system',
   type=system,
   sealed='both',
   postcheck = sys_check_refs,
   dict={
      subsystems = subsystems_spec,
      imports=imports_spec,
      blocks=blocks_spec,
      connections=connections_spec,
      node_configurations=node_config_spec,
      configurations=configs_spec,
      _parent=AnySpec{},
      _name=AnySpec{},
      _fqn=AnySpec{},
   },
   optional={ 'subsystems', 'imports', 'blocks', 'connections',
	      'node_configurations', 'configurations',
	      '_name', '_parent', '_fqn' },
}

-- add self references to subsystems dictionary
system_spec.dict.subsystems.dict.__other = { system_spec }

--- return the fqn of system s
-- @param sys system whos fqn to determine
-- @return fqn string
local function sys_fqn_get(sys)
   local fqn = {}
   local function __fqn_get(s)
      if s._parent == nil then return
      else __fqn_get(s._parent) end
      fqn[#fqn+1] = s._name
      fqn[#fqn+1] = '/'
   end

   assert(M.is_system(sys), "fqn_get: argument not a system")
   __fqn_get(sys)
   return table.concat(fqn)
end


--- Resolve config and connection references to blocks
-- checking will be done in the check function
-- @param root_sys
local function resolve_refs(root_sys)
   local function connref_resolve(sys, connref)
      local bref, portname = unpack(utils.split(connref, "%."))
      local btab = blocktab_get(sys, bref)
      return btab, portname
   end

   mapconfigs(
      function(c,_,p)
	 c._tgt = blocktab_get(p, c.name)
      end, root_sys)

   mapconns(
      function(c,_,p)
	 c._src, c._srcport = connref_resolve(p, c.src)
	 c._tgt, c._tgtport = connref_resolve(p, c.tgt)
      end, root_sys)
end

--- system_populate_meta
-- populate the meta data of a system
-- can be called multiple times
-- @param system
local function system_populate_meta(self)
   self.imports = self.imports or {}
   self.blocks = self.blocks or {}
   self.node_configurations = self.node_configurations or {}
   self.configurations = self.configurations or {}
   self.connections = self.connections or {}

   mapsys(
      function(s,n,p)
	 if p == nil then s._name='root' else s._name = n end
	 if p ~= nil then s._parent = p end
	 s._fqn = sys_fqn_get(s)
      end, self)


   mapblocks(
      function(b,i,p)
	 local mt = {
	    _parent = p,
	    _index = i,
	    _fqn = p._fqn .. b.name
	 }
	 mt.__index = mt
	 mt.__newindex = mt
	 setmetatable(b, mt)
      end, self)

   mapconfigs(
      function(c,i,p)
	 local mt = {
	    _parent = p,
	    _index = i,
	    _fqn = p._fqn..'configurations['..ts(i)..']'
	 }
	 mt.__index = mt
	 mt.__newindex = mt
	 setmetatable(c, mt)
      end, self)

   mapconns(
      function(c,i,p)
	 local mt = {
	    _parent = p,
	    _index = i,
	    _fqn = p._fqn..'connections['..ts(i)..']'
	 }
	 mt.__index = mt
	 mt.__newindex = mt
	 setmetatable(c, mt)
      end, self)

   -- just try to resolv, checking will complain if unresolved
   -- references remain
   resolve_refs(self)
end

--- System constructor
function system:init()
   system_populate_meta(self)
end

local function is_system(s)
   return umf.uoo_type(s) == 'instance' and umf.instance_of(system, s)
end

--- Validate a blockdiagram model.
function system:validate(verbose)
   return umf.check(self, system_spec, verbose)
end

--- read blockdiagram system file from usc or json file
-- @param fn file name of file (usc or json)
-- @param file_type optional file type (usc|json). Auto-detected from extension if not provided
local function load(fn, file_type)

   local function read_json()
      local f = assert(io.open(fn, "r"))
      local data = json.decode(f:read("*all"))
      return system(data)
   end

   if file_type == nil then
      file_type = string.match(fn, "^.+%.(.+)$")
   end

   local suc, mod
   if file_type == 'json' then
      if not has_json then
	 print("no cjson library found, unable to load json")
	 os.exit(1)
      end
      suc, mod = pcall(read_json, fn)
   elseif file_type == 'usc' or file_type == 'lua' then
      suc, mod = pcall(dofile, fn)
   else
      print("ubx-launch error: unknown file type "..ts(file_type))
      os.exit(1)
   end

   if not is_system(mod) then
      print("failed to load "..ts(fn).."\n"..ts(mod))
      os.exit(1)
   end

   return mod
end


---
--- late checking
---

--- check for unconnected input ports
local function lc_unconn_inports(ni, res)
   local function blk_chk_unconn_inports(b)
      ubx.ports_foreach(b,
			function(p)
			   if p.in_interaction == nil then
			      res[#res+1] = yellow("unconnected input port ", true) ..
				 green(safets(b.name)) .. "." ..
				 cyan(safets(p.name))
			   end
			end, ubx.is_inport)
   end
   ubx.blocks_map(ni, blk_chk_unconn_inports, ubx.is_cblock_instance)
end

--- check for unconnected output ports
local function lc_unconn_outports(ni, res)
   local function blk_chk_unconn_outports(b)
      ubx.ports_foreach(b,
			function(p)
			   if p.out_interaction == nil then
			      res[#res+1] = yellow("unconnected output port ", true) ..
				 green(safets(b.name)) .. "." ..
				 cyan(safets(p.name))
			   end
			end, ubx.is_outport)
   end
   ubx.blocks_map(ni, blk_chk_unconn_outports, ubx.is_cblock_instance)
end

-- table of late checks
-- this is used by by ubx-launch for help and cmdline handling
M._checks = {
   unconn_inports = {
      fun=lc_unconn_inports, desc="warn about unconnected input ports"
   },
   unconn_outports = {
      fun=lc_unconn_outports, desc="warn about unconnected output ports"
   }
}

--- Run a late validation
-- @param conf launch configuration
-- @param ni ubx_node_info
local function late_checks(conf, ni)
   if not conf.checks then return end
   local res = {}
   info("running late validation (%s)", table.concat(conf.checks, ", "))
   foreach(
      function (chk)
	 if not M._checks[chk] then
	    err_exit(1, "unknown check %s", chk)
	 end
	 M._checks[chk].fun(ni,res)
      end,
      conf.checks or {})

   foreach(function(v) warn(v) end, res)

   if #res > 0 and conf.werror then
      err_exit(1, "warnings raised and treating warnings as errors")
   end
end

--- is active block instance predicate
local function is_active_inst(b)
   return ubx.is_instance(b) and ubx.block_isactive(b)
end

--- is inactive block instance predicate
local function is_inactive_inst(b)
   return ubx.is_instance(b) and not ubx.block_isactive(b)
end

--- Start up a system
-- @param self system specification to load
-- @param ni node info
function system.startup(self, ni)

   local function block_start(b)
      local bname = safets(b.name)
      info("starting block %s", green(bname))
      local ret = ubx.block_tostate(b, 'active')
      if ret ~= 0 then
	 err_exit(ret, "failed to start block %s: %s",
		  bname, (ubx.retval_tostr[ret] or ts(ret)))
      end
   end

   -- start all non-active blocks
   ubx.blocks_map(ni, block_start, is_inactive_inst)

   -- start the active blocks
   ubx.blocks_map(ni, block_start, is_active_inst)
end

-- Pulldown a blockdiagram system
-- @param self system specification to load
-- @param ni configuration table
function system.pulldown(self, ni)

   local function block_stop (b)
      info("stopping active block %s", safets((b.name)))
      ubx.block_tostate(b, 'inactive')
   end

   -- stop all active blocks first
   ubx.blocks_map(ni, block_stop, is_active_inst)

   info("unloading all blocks")
   ubx.node_rm(ni)
end

--- Find and return ubx_block ptr
-- @param ni node_info
-- @param btab block table
-- @return bptr
local function get_ubx_block(ni, btab)
   assert(btab._fqn, "missing fqn entry of " .. ts(btab))
   return ubx.block_get(ni, btab._fqn)
end

--- load the systems modules
-- ensure modules are only loaded once
-- @param ni ubx_node_info into which to import
-- @param s system
local function import_modules(ni, s)
   local loaded = {}

   mapimports(
      function(m)
	 if loaded[m] then return
	 else
	    ubx.load_module(ni, m)
	    loaded[m]=true
	 end
      end, s)
end

--- Instantiate blocks
-- @param ni ubx_node_info into which to instantiate the blocks
-- @param root_sys system
local function create_blocks(ni, root_sys)
   mapblocks(
      function(b,i,p)
	 info("creating block %s [%s]", green(b._fqn), blue(b.type))
	 ubx.block_create(ni, b.type, b._fqn)
      end, root_sys)
end


---
--- configuration handling
---

--- Instantiate ubx_data for all node configurations incl. subsystems
-- NCs defined higher in the composition tree override lower ones.
-- @param root_sys root system
-- @return table of initialized config-name=ubx_data tuples
local function build_nodecfg_tab(ni, root_sys)
   local NC = {}

   -- instantiate a node config and add it to global table. skip it if
   -- a config of the same name exists
   local function create_nc(cfg, name, s)
      if NC[name] then
	 notice("node config %s in %s shadowed by a higher one", name, s._fqn)
	 return
      end

      local d = ubx.data_alloc(ni, cfg.type, 1)
      ubx.data_set(d, cfg.config, true)
      NC[name] = d
      info("creating node config %s [%s] %s",
	   blue(name), magenta(cfg.type),
	   yellow(utils.tab2str(cfg.config)))
   end

   mapndconfigs(create_nc, root_sys)
   return NC
end

--- Substitute #blockname with corresponding ubx_block_t ptrs
-- @param ni node_info
-- @param c configuration
-- @param s parent system
local function preproc_configs(ni, c, s)

   local function replace_hash(val, tab, key)
      local name = string.match(val, ".*#([%w_%-%/]+)")
      if not name then return end
      local bfqn = s._fqn..name
      local ptr = ubx.block_get(ni, bfqn)
      if ptr==nil then
	 err_exit(1, "error: failed to resolve # blockref to block %s", bfqn)
      elseif ubx.is_proto(ptr) then
	 err_exit(1, "error: block #%s is a proto block", bfqn)
      end
      info("resolved # blockref to %s", magenta(bfqn))
      tab[key]=ptr
   end

   utils.maptree(replace_hash, c.config, function(v,_) return type(v)=='string' end)
end

--- apply a single configuration to a block
--
-- This can be a regular or a node config value. In the latter case,
-- it will be looked up in NC. This function assumes that the config
-- exists.
--
-- @param b ubx_block_t*
-- @param name configuration name
-- @param val configuration value
-- @param NC node configuration table
local function apply_cfg_val(b, name, val, NC)
   local blkcfg = ubx.block_config_get(b, name)
   local blkfqn = safets(b.name)

   -- check for references to node configs
   local nodecfg = check_noderef(val)

   if nodecfg then
      if not NC[nodecfg] then
	 err_exit(1, "invalid node config reference '%s'", val)
      end
      info("nodecfg %s.%s with %s %s",
	   green(blkfqn), blue(name), yellow(nodecfg),
	   yellow(utils.tab2str(NC[nodecfg])))

      local ret = ubx.config_assign(blkcfg, NC[nodecfg])
      if ret < 0 then
	 err_exit(1, "failed to assign nodecfg %s to %s: %s",
		  utils.tab2str(NC[nodecfg]),
		  green(blkfqn.."."..blue(name)),
		  ubx.retval_tostr[ret])
      end
   else -- regular config
      info("cfg %s.%s: %s", green(blkfqn), blue(name), yellow(utils.tab2str(val)))
      ubx.set_config(b, name, val)
   end
end

--- Configure a the given block with a config
-- (scalar, table or node cfg reference '&ndcfg'
-- @param cfg config table
-- @param b ubx_block_t
-- @param NC global config table
-- @param configured table to remember already configured block-configs
-- @param nonexist table to remember nonexisting configurations
local function apply_config(cfg, b, NC, configured, nonexist)

   for name,val in pairs(cfg.config) do
      local cfgfqn = cfg._tgt._fqn..'.'..name

      if ubx.block_config_get(b, name) == nil then
	 nonexist[cfgfqn] = true
	 goto continue
      end

      if configured[cfgfqn] then
	 notice("skipping config %s: already configured with %s", cfgfqn, utils.tab2str(val))
	 goto continue
      end

      apply_cfg_val(b, name, val, NC)
      configured[cfgfqn] = true

      ::continue::
   end
end

--- Configure the previously unconfigured configs
--
-- This function is to be called after init and is intended to
-- configure dynamically added configs. Thus, it will ignore all
-- configurations that are not in the nonexist table
--
-- @param cfg config table
-- @param b ubx_block_t
-- @param NC global config table
-- @param configured table to remember already configured block-configs
-- @param nonexist table to remember nonexisting configurations
local function reapply_config(cfg, b, NC, configured, nonexist)
   for name,val in pairs(cfg.config) do
      local cfgfqn = cfg._tgt._fqn..'.'..name

      -- skip all but the previously non-existing. Don't exit for the
      -- previously configured ( nonexist[] = false ) so that we get
      -- the "skipping already configured" message
      if nonexist[cfgfqn] == nil then goto continue end

      if ubx.block_config_get(b, name) == nil then
	 err_exit(1, "block %s [%s] has no config '%s'",
		  cfg._tgt._fqn, cfg._tgt.type, name)
	 nonexist[cfgfqn] = false
	 goto continue
      end

      notice("late config of %s with %s", cfgfqn, utils.tab2str(val))

      if configured[cfgfqn] then
	 notice("skipping config %s: already configured with %s", cfgfqn, utils.tab2str(val))
	 goto continue
      end

      apply_cfg_val(b, name, val, NC)
      configured[cfgfqn] = true
      ::continue::
   end

end

--- configure all blocks
-- @param ni node_info
-- @param root_sys root system
-- @param NC
local function configure_blocks(ni, root_sys, NC)

   -- table of already configured configs
   -- { [<blkfqn.config>] = true }
   -- to avoid double configuration
   local configured = {}

   -- table of configurations that were skipped because
   -- non-existant. These will be retried in reapply_config to handle
   -- dynamically created configurations that will be interpreted in
   -- the start hook.
   local nonexist = {}

   -- substitue #blockname syntax
   mapconfigs(function(c, i, s) preproc_configs(ni, c, s, i) end, root_sys)

   -- apply configurations to blocks
   mapconfigs(
      function(cfg, i)
	 local b = get_ubx_block(ni, cfg._tgt)
	 if b == nil then
	    err_exit(1, "error: config %s for block %s: no such block found",
		     cfg._fqn, cfg.name)
	 end

	 local bstate = b:get_block_state()
	 if bstate ~= 'preinit' then
	    warn("applying configs: block %s not in state preinit but %s", cfg._tgt._fqn, bstate)
	    return
	 end

	 apply_config(cfg, b, NC, configured, nonexist)
      end, root_sys)

   -- initialize all blocks (brings them to state 'inactive')
   mapblocks(
      function(btab,i,p)
	 local b = get_ubx_block(ni, btab)
	 info("initializing block %s", safets(b.name))
	 local ret = ubx.block_init(b)
	 if ret ~= 0 then
	    err_exit(ret, "failed to initialize block %s: %d",
		     btab.name, tonumber(ret))
	 end
      end, root_sys)

   -- reapply configurations to blocks
   mapconfigs(
      function(cfg, i)
	 local b = get_ubx_block(ni, cfg._tgt)
	 if b == nil then
	    err_exit(1, "error: config %s for block %s: no such block found",
		     cfg._fqn, cfg.name)
	 end

	 local bstate = b:get_block_state()

	 if bstate ~= 'inactive' then
	    warn("reapplying configs: block %s not in state preinit but %s",
		 cfg._tgt._fqn, bstate)
	    return
	 end

	 reapply_config(cfg, b, NC, configured, nonexist)
      end, root_sys)

   -- check that all initially non-existing configs were configured
   for cfgfqn, val in pairs(nonexist) do
      assert(val==true, fmt("unconfigured nonexist config %s reapply", cfgfqn))
   end

end

--- Connect blocks
-- @param ni node_info
-- @param root_sys root system
local function connect_blocks(ni, root_sys)
   local function do_connect(c, i, p)
      local srcblk = c._src._fqn
      local srcport = c._srcport
      local tgtblk = c._tgt._fqn
      local tgtport = c._tgtport

      -- are we connecting to an interaction?
      if srcport==nil then
	 -- src is interaction, target a port
	 local ib = ubx.block_get(ni, srcblk)

	 if ib==nil then
	    err_exit(1, "do_connect: unknown src block %s", srcblk)
	 end

	 if not ubx.is_iblock_instance(ib) then
	    err_exit(1, "%s is not a valid iblock instance", srcblk)
	 end

	 local btgt = ubx.block_get(ni, tgtblk)
	 local ptgt = ubx.port_get(btgt, tgtport)

	 if ubx.port_connect_in(ptgt, ib) ~= 0 then
	    err_exit(1, "failed to connect interaction %s to port %s.%s",
			srcblk, tgtblk, tgtport)
	 end
	 info("connecting %s (iblock) ->  %s.%s", srcblk, tgtblk, tgtport)
      elseif tgtport==nil then
	 -- src is a port, target is an interaction
	 local ib = ubx.block_get(ni, tgtblk)

	 if ib==nil then
	    err_exit(1, "do_connect: unknown tgt block %s", tgtblk)
	 end

	 if not ubx.is_iblock_instance(ib) then
	    err_exit(1, "%s not a valid iblock instance", tgtblk)
	 end

	 local bsrc = ubx.block_get(ni, srcblk)
	 local psrc = ubx.port_get(bsrc, srcport)

	 if ubx.port_connect_out(psrc, ib) ~= 0 then
	    err_exit(1, "failed to connect %s.%s to %s (iblock)",
		     srcblk, srcport, tgtblk)
	 end
	 info("connecting %s.%s -> %s (iblock)", srcblk, srcport, tgtblk)
      else
	 -- both src and target are ports
	 local bufflen = c.buffer_length or 1

	 info("connecting %s.%s -[%d]-> %s.%s",
	      srcblk, srcport, bufflen, tgtblk, tgtport)

	 local bsrc = ubx.block_get(ni, srcblk)
	 local btgt = ubx.block_get(ni, tgtblk)

	 if bsrc==nil then
	    err_exit(1, "ERR: src block %s not found", srcblk)
	 end

	 if btgt==nil then
	    err_exit(1, "ERR: tgt block %s not found", tgtblk)
	 end
	 ubx.conn_lfds_cyclic(bsrc, srcport, btgt, tgtport, bufflen)
      end
   end
   mapconns(do_connect, root_sys)
end

--- Merge one system into another
-- No duplicate handling. Running the validation after a merge is strongly recommended.
-- @param self targed of the merge
-- @param sys system which to merge into self
function system.merge(self, sys)
   local function do_merge(dest, src)
      foreach(function (imp) insert(dest.imports, imp) end, src.imports)
      foreach(function (blk) insert(dest.blocks, blk) end, src.blocks)
      foreach(function (cfg) insert(dest.configurations, cfg) end, src.configurations)
      foreach(function (conn) insert(dest.connections, conn) end, src.connections)

      foreach(
	 function (ndcfg, name)
	    if dest.node_configurations[name] then
	       warn("merge: overriding existing destination system node_config %s", name)
	    end
	    dest.node_configurations[name] = ndcfg
	 end, src.node_configurations)

      if src.subsystems and not dest.subsystems then dest.subsystems={} end

      foreach(
	 function (subsys, name)
	    if dest.subsystems[name] then
	       warn("merge: overriding existing destination subsystem %s", name)
	    end
	    dest.subsystems[name] = subsys
	 end, src.subsystems)

      if src.subsystems then
	 do_merge(dest.subsystems, src.subsystems)
      end
   end

   do_merge(self, sys)
end

--- Launch a blockdiagram system
-- @param self system specification to load
-- @param t configuration table
-- @return ni node_info handle
function system.launch(self, t)

   if self:validate(false) > 0 then self:validate(true) os.exit(1) end

   -- fire it up
   t = t or {}
   t.nodename = t.nodename or "n"
   USE_STDERR = t.use_stderr or false

   local ni = ubx.node_create(t.nodename,
			      { loglevel=t.loglevel,
				mlockall=t.mlockall,
				dumpable=t.dumpable })

   def_loggers(ni, "launch")
   import_modules(ni, self)
   create_blocks(ni, self)
   _NC = build_nodecfg_tab(ni, self)
   configure_blocks(ni, self, _NC)
   connect_blocks(ni, self)
   late_checks(t, ni)

   if not t.nostart then system.startup(self, ni) end

   return ni
end

-- exports
M.is_system = is_system
M.system = system
M.system_spec = system_spec
M.load = load

return M
