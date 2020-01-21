--
-- blockdiagram: DSL for specifying and launching microblx system
-- compositions
--
-- Copyright (C) 2013 Markus Klotzbuecher <markus.klotzbuecher@mech.kuleuven.be>
-- Copyright (C) 2014-2018 Markus Klotzbuecher <mk@mkio.de>
--
-- SPDX-License-Identifier: BSD-3-Clause
--

local ubx = require "ubx"
local umf = require "umf"
local utils = require "utils"
local has_json, json = pcall(require, "cjson")
local ts = tostring
local M={}

local strict = require "strict"

-- shortcuts
local foreach = utils.foreach
local ts = tostring

-- node configuration
_NC = nil

local red=ubx.red
local blue=ubx.blue
local cyan=ubx.cyan
local green=ubx.green
local yellow=ubx.yellow
local magenta=ubx.magenta

local crit, err, warn, notice, info = nil, nil, nil, nil, nil

--- Create logging helper functions.
local function def_loggers(ni, src)
   crit = function(msg) ubx.crit(ni, src, msg) end
   err = function(msg) ubx.err(ni, src, msg) end
   warn = function(msg) ubx.warn(ni, src, msg) end
   notice = function(msg) ubx.notice(ni, src, msg) end
   info = function(msg) ubx.info(ni, src, msg) end
end

local function err_exit(code, msg)
   err(msg)
   os.exit(code)
end

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
   postcheck=function(self, obj, vres)
      -- extra checks
      return true
   end
}

-- blocks
local blocks_spec = TableSpec
{
   name='blocks',
   array = {
      TableSpec
      {
	 name='block',
	 dict = { name=StringSpec{}, type=StringSpec{}, _parent=AnySpec{}, },
	 sealed='both',
	 optional = { '_parent' }
      }
   },
   sealed='both',
}

-- connections
local connections_spec = TableSpec
{
   name='connections',
   array = {
      TableSpec
      {
	 name='connection',
	 dict = {
	    src=StringSpec{},
	    tgt=StringSpec{},
	    buffer_length=NumberSpec{ min=0 }
	 },
	 sealed='both',
	 optional={'buffer_length'},
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
local configs_spec = TableSpec
{
   name='configurations',

   -- regular block configs
   array = {
      TableSpec
      {
	 name='configuration',
	 dict = { name=StringSpec{}, config=AnySpec{}, _parent=AnySpec{} },
	 sealed='both',
	 optional = { '_parent' }
      },
   },
   sealed='both'
}

-- start
local start_spec = TableSpec
{
   name='start',
   sealed='both',
   array = { StringSpec },
}

local subsystems_spec = TableSpec {
   name='subsystems',
   dict={}, -- self reference added below
   sealed='both',
}

--- system spec
local system_spec = ObjectSpec
{
   name='system',
   type=system,
   sealed='both',
   dict={
      subsystems = subsystems_spec,
      imports=imports_spec,
      blocks=blocks_spec,
      connections=connections_spec,
      node_configurations=node_config_spec,
      configurations=configs_spec,
      start=start_spec,
      _parent=AnySpec{},
      _name=StringSpec{},
   },
   optional={ 'subsystems', 'imports', 'blocks', 'connections',
	      'node_configurations', 'configurations', 'start',
	      '_parent', '_name' },
}

-- add self references to subsystems dictionary
system_spec.dict.subsystems.dict.__other = { system_spec }


--- apply func to systems including all subsystems
-- @param func function(system, name, parent-system)
-- @param root root system
-- @return list of return values of func
local function mapsys(func, root)
   local res = {}

   local function __mapsys(sys,name,psys)
      res[#res+1] = func(sys, name, psys)
      for k,s in pairs(sys.subsystems) do __mapsys(s, k, sys) end
   end
   __mapsys(root)
   return res
end


--- Apply func to all objs of system[systab]
-- @param func function(obj, index, parent_system)
-- @param root root system
-- @param systab which system table to map (blocks, configurations, ...)
-- @return list of return values of func
local function mapobj(func, root_sys, systab)
   local res = {}

   local function __mapobj(s)
      for i,o in ipairs(s[systab]) do
	 res[#res+1] = func(o, i, s)
      end
   end
   mapsys(__mapobj, root_sys)
end

local function mapblocks(func, root_sys) return mapobj(func, root_sys, 'blocks') end
local function mapconns(func, root_sys) return mapobj(func, root_sys, 'connections') end
local function mapconfigs(func, root_sys) return mapobj(func, root_sys, 'configurations') end
local function mapimports(func, root_sys) return mapobj(func, root_sys, 'imports') end

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

--- return the fqn of block b
-- @param b block whos fqn to determine
-- @return fqn string
local function block_fqn_get(b)
   return sys_fqn_get(b._parent)..b.name
end

--- System constructor
function system:init()
   self.imports = self.imports or {}
   self.subsystems = self.subsystems or {}
   self.blocks = self.blocks or {}
   self.node_configurations = self.node_configurations or {}
   self.configurations = self.configurations or {}
   self.connections = self.connections or {}
   self.start = self.start or {}

   -- create system _name fields
   mapsys(
      function(s,n,p)
	 if p==nil then s._name='root' else s._name = n end
      end, self)

   -- add system _parent links
   mapsys(function(s,n,p) if p ~= nil then s._parent = p end end, self)

   -- add _parent links
   mapblocks(function(b,_,p) b._parent = p end, self)
   mapconfigs(function(c,_,p) c._parent = p end, self)
end

--- Convert a config to a string (for debugging)
-- @param cfg config table
-- @return string
local function config_tostr(cfg, idx)
   local sfqn =	sys_fqn_get(cfg._parent)
   return string.format("config %s #%i for block %s", sfqn, idx, cfg.name)
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
-- @param file_type optional file type - either \em usc or \em json. If not specified, the type will be auto-detected from the extension of the file
-- @return system model or will exit(1)
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
      print("ubx_launch error: unknown file type "..tostring(file_type))
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
				 green(ubx.safe_tostr(b.name)) .. "." ..
				 cyan(ubx.safe_tostr(p.name))
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
				 green(ubx.safe_tostr(b.name)) .. "." ..
				 cyan(ubx.safe_tostr(p.name))
			   end
			end, ubx.is_outport)
   end
   ubx.blocks_map(ni, blk_chk_unconn_outports, ubx.is_cblock_instance)
end

-- table of late checks
-- this is used by by ubx_launch for help and cmdline handling
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
   info("running late validation ("..
	   table.concat(conf.checks, ", ")..")")
   foreach(
      function (chk)
	 if not M._checks[chk] then
	    err_exit(-1, "unknown check "..chk)
	 end
	 M._checks[chk].fun(ni,res)
      end,
      conf.checks or {})

   foreach(function(v) warn(v) end, res)

   if #res > 0 and conf.werror then
      err_exit(-1, "warnings raised and treating warnings as errors")
   end
end


--- Pulldown a blockdiagram system
-- @param self system specification to load
-- @param t configuration table
function system.pulldown(self, ni)
   if #self.start > 0 then
      -- stop the start table blocks in reverse order
      for i = #self.start, 1, -1 do
	 info("stopping " .. green(self.start[i]))
	 ubx.block_unload(ni, self.start[i])
      end
   end
   info("stopping remaining blocks")
   ubx.node_rm(ni)
end


--- Start up a system
-- @param self system specification to load
-- @param t configuration table
-- @return ni node_info handle
function system.startup(self, ni)
   foreach(
      function (bmodel)

	 -- skip the blocks in the start table
	 if utils.table_has(self.start, bmodel.name) then return end
	 local b = ubx.block_get(ni, bmodel.name)
	 info("starting ".. green(bmodel.name))

	 local ret = ubx.block_tostate(b, 'active')
	 if ret ~= 0 then
	    err_exit(ret, "failed to start block "..bmodel.name..": "..ubx.retval_tostr[ret])
	 end
      end, self.blocks)
   -- start the start table blocks in order
   for _,trigname in ipairs(self.start) do
      local b = ubx.block_get(ni, trigname)
      info("starting ".. green(trigname))
      local ret = ubx.block_tostate(b, 'active')
      if ret ~= 0 then
	 err_exit(ret, "failed to start block "..trigname..": ", ret)
      end
   end
end


--- Find and return ubx_block ptr
-- @param ni node_info
-- @param obj config or block table
local function find_block(ni, obj)
   assert(obj.name, "missing name entry of " .. tostring(obj))
   assert(obj._parent, "missing _parent link of ".. tostring(obj.name))
   local bfqn = sys_fqn_get(obj._parent)..obj.name
   local bptr = ubx.block_get(ni, bfqn)
   return bptr
end

--- Instantiate ubx_data for all node configurations incl. subsystems
-- NCs defined higher in the composition tree override lower ones.
-- @param root_sys root system
-- @return table of initialized config-name=ubx_data tuples
local function build_nodecfg_tab(ni, root_sys)
   local NC = {}
   local queue = { root_sys }

   -- instantiate node config and add it to global table
   -- skip it and if a config of the same name exists
   local function __create_nc(cfg, name, s)
      if NC[name] then
	 notice("node config "..sys_fqn_get(s)..name.." shadowed by a higher one")
	 return
      end

      local d = ubx.data_alloc(ni, cfg.type, 1)
      ubx.data_set(d, cfg.config, true)
      NC[name] = d
      info("creating node config "..blue(name)..
	      "["..magenta(cfg.type).."] "..
	      yellow(utils.tab2str(cfg.config)))
   end

   -- breadth first
   while #queue > 0 do
      local next = table.remove(queue, 1)

      -- process all nc's of s
      foreach(
	 function (nc, name)
	    __create_nc(nc, name, next)
	 end, next.node_configurations)

      -- add s's subsystems to queue
      foreach(function (subsys) queue[#queue+1] = subsys end, next.subsystems)
   end

   return NC
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
	    info("importing module "..magenta(m))
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
	 local bfqn = block_fqn_get(b)
	 info("creating block "..green(bfqn).." ["..blue(b.type).."]")
	 ubx.block_create(ni, b.type, bfqn)
      end, root_sys)
end


---
--- configuration handling
---

--- Substitute #blockname with corresponding ubx_block_t ptrs
-- @param ni node_info
-- @param c configuration
local function preproc_configs(ni, c, s)
   local function replace_hash(val, tab, key)
      local name = string.match(val, ".*#([%w_-%/]+)")
      if not name then return end
      local bfqn = sys_fqn_get(s)..name
      local ptr = ubx.block_get(ni, bfqn)
      if ptr==nil then
	 err_exit(-1, "error: failed to resolve # blockref to block "..bfqn)
      elseif ubx.is_proto(ptr) then
	 err_exit("error: block #"..bfqn.." is a proto block")
      end
      info(magenta("resolved # blockref to "..bfqn))
      tab[key]=ptr
   end

   utils.maptree(replace_hash, c.config, function(v,_) return type(v)=='string' end)
end

--- Configure a the given block with a config
-- (scalar, table or node cfg reference '&ndcfg'
-- @param c blockdiagram.config
-- @param b ubx_block_t
-- @param NC global config table
local function apply_config(blkconf, b, NC)
   local function check_noderef(val)
      if type(val) ~= 'string' then return end
      return string.match(val, "^%s*&([%w_-]+)")
   end

   for name,val in pairs(blkconf.config) do
      -- check for references to node configs
      local nodecfg = check_noderef(val)

      if nodecfg then
	 if not NC[nodecfg] then
	    err_exit(1, "invalid node config reference '"..val.."'")
	 end
	 local blkcfg = ubx.block_config_get(b, name)

	 if blkcfg==nil then
	    err_exit(1, "non-existing config '"..blkconf.name.. "."..name.."'")
	 end

	 info("cfg "..green(blkconf.name.."."..blue(name)).." -> node cfg "..yellow(nodecfg))
	 ubx.config_assign(blkcfg, NC[nodecfg])
      else -- regular config
	 info("cfg "..green(blkconf.name).."."..blue(name)..": "..yellow(utils.tab2str(val)))
	 ubx.set_config(b, name, val)
      end
   end
end

--- Configure apply the given cfg to it's block in ni
-- @param ni node_info
-- @param cfg system.config table
-- @param NC node (global) configuration table
local function configure_block(ni, cfg, NC, i)
   local b = find_block(ni, cfg)

   if b==nil then
      err_exit(-1, "error: config "..config_tostr(cfg, i).." no ubx block instance found")
   end

   local bstate = b:get_block_state()
   if bstate ~= 'preinit' then
      warn("block "..bfqn.." not in state preinit but "..bstate)
      return
   end

   apply_config(cfg, b, NC)
end

--- configure all blocks
-- @param ni node_info
-- @param root_sys root system
-- @param NC
local function configure_blocks(ni, root_sys, NC)

   -- substitue #blockname syntax
   mapconfigs(function(c, i, s) preproc_configs(ni, c, s, i) end, root_sys)

   -- apply configurations to blocks
   mapconfigs(function(c, i) configure_block(ni, c, NC, i) end, root_sys)

   -- configure all blocks
   mapblocks(
      function(btab,i,p)
	 local b = find_block(ni, btab)
	 info("running block "..ubx.safe_tostr(b.name).." init")
	 local ret = ubx.block_init(b)
	 if ret ~= 0 then
	    err_exit(ret, "failed to initialize block "..btab.name..": "..tonumber(ret))
	 end
      end, root_sys)
end


--- Launch a blockdiagram system
-- @param self system specification to load
-- @param t configuration table
-- @return ni node_info handle
function system.launch(self, t)
   if self:validate(false) > 0 then
      self:validate(true)
      os.exit(1)
   end

   -- Instatiate the system recursively.
   -- @param self system
   -- @param t global configuration
   -- @param ni node_info
   local function __launch(self, t, ni)
      info("launching system in node "..ts(t.nodename))

      -- create connections
      if #self.connections > 0 then
	 foreach(function(c)
	       local bnamesrc,pnamesrc = unpack(utils.split(c.src, "%."))
	       local bnametgt,pnametgt = unpack(utils.split(c.tgt, "%."))

	       -- are we connecting to an interaction?
	       if pnamesrc==nil then
		  -- src="iblock", tgt="block.port"
		  local ib = ubx.block_get(ni, bnamesrc)

		  if ib==nil then
		     err_exit(1, "unkown block "..ts(bnamesrc))
		  end

		  if not ubx.is_iblock_instance(ib) then
		     err_exit(1, ts(bnamesrc).." not a valid iblock instance")
		  end

		  local btgt = ubx.block_get(ni, bnametgt)
		  local ptgt = ubx.port_get(btgt, pnametgt)

		  if ubx.port_connect_in(ptgt, ib) ~= 0 then
		     err_exit(1, "failed to connect interaction "..bnamesrc..
				 " to port "..ts(bnametgt).."."..ts(pnametgt))
		  end
		  info("connecting "..green(bnamesrc).." (iblock) -> "
			  ..green(ts(bnametgt)).."."..cyan(ts(pnametgt)))

	       elseif pnametgt==nil then
		  -- src="block.port", tgt="iblock"
		  local ib = ubx.block_get(ni, bnametgt)

		  if ib==nil then
		     err_exit(1, "unkown block "..ts(bnametgt))
		  end

		  if not ubx.is_iblock_instance(ib) then
		     err_exit(1, ts(bnametgt).." not a valid iblock instance")
		  end

		  local bsrc = ubx.block_get(ni, bnamesrc)
		  local psrc = ubx.port_get(bsrc, pnamesrc)

		  if ubx.port_connect_out(psrc, ib) ~= 0 then
		     err_exit(1, "failed to connect "
				 ..ts(bnamesrc).."." ..ts(pnamesrc).." to "
				 ..ts(bnametgt).." (iblock)")
		  end
		  info("connecting "..green(ts(bnamesrc)).."."..cyan(ts(pnamesrc)).." -> "
			  ..green(bnametgt).." (iblock)")
	       else
		  -- standard connection between two cblocks
		  local bufflen = c.buffer_length or 1

		  info("connecting "..green(ts(bnamesrc))..'.'..cyan(ts(pnamesrc))
			  .." -["..yellow(ts(bufflen), true).."]".."-> "
			  ..green(ts(bnametgt)).."."..cyan(ts(pnametgt)))

		  local bsrc = ubx.block_get(ni, bnamesrc)
		  local btgt = ubx.block_get(ni, bnametgt)

		  if bsrc==nil then
		     err_exit(1, "ERR: no block named "..bnamesrc.. " found")
		  end

		  if btgt==nil then
		     err_exit(1, "ERR: no block named "..bnametgt.. " found")
		  end
		  ubx.conn_lfds_cyclic(bsrc, pnamesrc, btgt, pnametgt, bufflen)
	       end
		 end, self.connections)
      end

      -- run late checks
      late_checks(t, ni)

      -- startup
      if not t.nostart then system.startup(self, ni) end
   end

   -- fire it up
   t = t or {}
   t.nodename = t.nodename or "n"

   local ni = ubx.node_create(t.nodename, t.loglevel)

   def_loggers(ni, "launch")
   import_modules(ni, self)
   create_blocks(ni, self)

   _NC = build_nodecfg_tab(ni, self)

   configure_blocks(ni, self, _NC)

   -- connect_blocks

   -- startup blocks
   -- start_system(ni, self)

   -- configure and connect
   __launch(self, t, ni)
   return ni
end

-- exports
M.is_system = is_system
M.system = system
M.system_spec = system_spec
M.load = load

return M
