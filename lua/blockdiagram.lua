--
-- blockdiagram: DSL for specifying and launching microblx system
-- compositions
--
-- Copyright (C) 2013 Markus Klotzbuecher <markus.klotzbuecher@mech.kuleuven.be>
-- Copyright (C) 2014-2019 Markus Klotzbuecher <mk@mkio.de>
--
-- SPDX-License-Identifier: BSD-3-Clause
--

local ubx = require "ubx"
local umf = require "umf"
local utils = require "utils"
local has_json, json = pcall(require, "cjson")
local M={}

local strict = require "strict"

-- shortcuts
local foreach = utils.foreach
local ts = tostring

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
   crit = function(msg) stderr(msg); ubx.crit(ni, src, msg) end
   err = function(msg) stderr(msg); ubx.err(ni, src, msg) end
   warn = function(msg) stderr(msg); ubx.warn(ni, src, msg) end
   notice = function(msg) stderr(msg); ubx.notice(ni, src, msg) end
   info = function(msg) stderr(msg); ubx.info(ni, src, msg) end
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
}

-- blocks
local blocks_spec = TableSpec
{
   name='blocks',
   array = {
      TableSpec
      {
	 name='block',
	 dict = { name=StringSpec{}, type=StringSpec{}, _x=AnySpec{}, },
	 sealed='both',
	 optional = { '_x' }
      }
   },
   sealed='both',
}

-- connections
local function conn_check_srctgt(class, conn, vres)
   local res = true
   if not conn._x.src then
      umf.add_msg(vres, "err", "invalid src block ".. conn.src)
      res = false
   end
   if not conn._x.tgt then
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
	    _x=AnySpec{},
	 },
	 sealed='both',
	 optional={'buffer_length', '_x'},
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
   if cfg._x and cfg._x.tgt then return true end
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
	    _x=AnySpec{} },
	 sealed='both',
	 optional = { '_x' }
      },
   },
   sealed='both'
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
      _x=AnySpec{},
   },
   optional={ 'subsystems', 'imports', 'blocks', 'connections',
	      'node_configurations', 'configurations', '_x' },
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

--- return the fqn of system s
-- @param sys system whos fqn to determine
-- @return fqn string
local function sys_fqn_get(sys)
   local fqn = {}
   local function __fqn_get(s)
      if s._x.parent == nil then return
      else __fqn_get(s._x.parent) end
      fqn[#fqn+1] = s._x.name
      fqn[#fqn+1] = '/'
   end

   assert(M.is_system(sys), "fqn_get: argument not a system")
   __fqn_get(sys)
   return table.concat(fqn)
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

--- Resolve config and connection references to blocks
-- checking will be done in the check function
-- @param root_sys
local function resolve_refs(root_sys)
   local function connref_resolve(sys, connref)
      local bref, portname = unpack(utils.split(connref, "%."))
      local btab = blocktab_get(sys, bref)
      return btab, portname
   end

   -- extend config _x tab with a ref to the target block
   mapconfigs(
      function(c,_,p)
	 c._x.tgt = blocktab_get(p, c.name)
      end, root_sys)

   -- conn _x = { src=tab, srcport=string, tgt=tab, tgtport=string }
   mapconns(
      function(c,_,p)
	 c._x.src, c._x.srcport = connref_resolve(p, c.src)
	 c._x.tgt, c._x.tgtport = connref_resolve(p, c.tgt)
      end, root_sys)
end

--- System constructor
function system:init()
   self.imports = self.imports or {}
   self.subsystems = self.subsystems or {}
   self.blocks = self.blocks or {}
   self.node_configurations = self.node_configurations or {}
   self.configurations = self.configurations or {}
   self.connections = self.connections or {}

   -- populate _x table with convenience data
   mapsys(
      function(s,n,p)
	 s._x = {}
	 if p == nil then s._x.name='root' else s._x.name = n end
	 if p ~= nil then s._x.parent = p end
	 s._x.fqn = sys_fqn_get(s)
      end, self)


   -- populate _x tables
   mapblocks(
      function(b,i,p)
	 b._x = {}
	 b._x.parent = p
	 b._x.index = i
	 b._x.fqn = p._x.fqn ..b.name
      end, self)

   mapconfigs(
      function(c,i,p)
	 c._x = {}
	 c._x.parent = p
	 c._x.index = i
	 c._x.fqn = p._x.fqn..'configurations['..ts(i)..']'
      end, self)

   mapconns(
      function(c,i,p)
	 c._x = {}
	 c._x.parent = p
	 c._x.index = i
	 c._x.fqn = p._x.fqn..'connections['..ts(i)..']'
      end, self)

   -- just try to resolv, checking will complain if unresolved
   -- references remain
   resolve_refs(self)
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
      print("ubx_launch error: unknown file type "..ts(file_type))
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


local function is_active_inst(b)
   return ubx.is_instance(b) and ubx.block_isactive(b)
end

local function is_inactive_inst(b)
   return ubx.is_instance(b) and not ubx.block_isactive(b)
end

--- Start up a system
-- @param self system specification to load
-- @param ni node info
function system.startup(self, ni)

   local function block_start(b)
      local bname = ubx.safe_tostr(b.name)
      info("starting ".. green(bname))
      local ret = ubx.block_tostate(b, 'active')
      if ret ~= 0 then
	 err_exit(ret, "failed to start block "..bname..": "..(ubx.retval_tostr[ret] or ts(ret)))
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
      info("stopping active block "..ubx.safe_tostr((b.name)))
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
   assert(btab._x.fqn, "missing fqn entry of " .. ts(btab))
   return ubx.block_get(ni, btab._x.fqn)
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
	 info("creating block "..green(b._x.fqn).." ["..blue(b.type).."]")
	 ubx.block_create(ni, b.type, b._x.fqn)
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
	 notice("node config "..name.." in "..s._x.fqn.." shadowed by a higher one")
	 return
      end

      local d = ubx.data_alloc(ni, cfg.type, 1)
      ubx.data_set(d, cfg.config, true)
      NC[name] = d
      info("creating node config "..blue(name)..
	      "["..magenta(cfg.type).."] "..
	      yellow(utils.tab2str(cfg.config)))
   end

   mapndconfigs(create_nc, root_sys)
   return NC
end

--- Substitute #blockname with corresponding ubx_block_t ptrs
-- @param ni node_info
-- @param c configuration
local function preproc_configs(ni, c, s)

   local function replace_hash(val, tab, key)
      local name = string.match(val, ".*#([%w_-%/]+)")
      if not name then return end
      local bfqn = s._x.fqn..name
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

	 info("configuring "..green(blkconf._x.tgt._x.fqn.."."..blue(name))
		 .." with nodecfg "..yellow(nodecfg).." "..yellow(utils.tab2str(NC[nodecfg])))
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
   local bfqn =	cfg._x.tgt._x.fqn
   local b = get_ubx_block(ni, cfg._x.tgt)

   if b==nil then
      err_exit(-1, "error: config "..cfg._x.fqn.." for block "..
		  cfg.name.."no ubx block instance found")
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
   local configured = {}

   -- substitue #blockname syntax
   mapconfigs(function(c, i, s) preproc_configs(ni, c, s, i) end, root_sys)

   -- apply configurations to blocks
   mapconfigs(
      function(c, i)
	 local bfqn = c._x.tgt._x.fqn
	 if configured[bfqn] then
	    info("skipping "..bfqn.." config "..utils.tab2str(c.config)..": already configured")
	    return
	 end
	 configure_block(ni, c, NC, i)
	 configured[bfqn] = true
      end, root_sys)

   -- configure all blocks
   mapblocks(
      function(btab,i,p)
	 local b = get_ubx_block(ni, btab)
	 info("running block "..ubx.safe_tostr(b.name).." init")
	 local ret = ubx.block_init(b)
	 if ret ~= 0 then
	    err_exit(ret, "failed to initialize block "..btab.name..": "..tonumber(ret))
	 end
      end, root_sys)
end

--- Connect blocks
-- @param ni node_info
-- @param root_sys root system
local function connect_blocks(ni, root_sys)
   local function do_connect(c, i, p)
      local srcblk = c._x.src._x.fqn
      local srcport = c._x.srcport
      local tgtblk = c._x.tgt._x.fqn
      local tgtport = c._x.tgtport

      -- are we connecting to an interaction?
      if c._x.srcport==nil then
	 -- src is interaction, target a port
	 local ib = ubx.block_get(ni, srcblk)

	 if ib==nil then
	    err_exit(1, "unkown block "..ts(srcblk))
	 end

	 if not ubx.is_iblock_instance(ib) then
	    err_exit(1, ts(srcblk).." not a valid iblock instance")
	 end

	 local btgt = ubx.block_get(ni, tgtblk)
	 local ptgt = ubx.port_get(btgt, tgtport)

	 if ubx.port_connect_in(ptgt, ib) ~= 0 then
	    err_exit(1, "failed to connect interaction "..srcblk..
			" to port "..ts(tgtblk).."."..ts(tgtport))
	 end
	 info("connecting "..green(srcblk).." (iblock) -> "
		 ..green(ts(tgtblk)).."."..cyan(ts(tgtport)))

      elseif tgtport==nil then
	 -- src is a port, target is an interaction
	 local ib = ubx.block_get(ni, tgtblk)

	 if ib==nil then
	    err_exit(1, "unkown block "..ts(tgtblk))
	 end

	 if not ubx.is_iblock_instance(ib) then
	    err_exit(1, ts(tgtblk).." not a valid iblock instance")
	 end

	 local bsrc = ubx.block_get(ni, srcblk)
	 local psrc = ubx.port_get(bsrc, srcport)

	 if ubx.port_connect_out(psrc, ib) ~= 0 then
	    err_exit(1, "failed to connect "
			..ts(srcblk).."." ..ts(srcport).." to "
			..ts(tgtblk).." (iblock)")
	 end
	 info("connecting "..green(ts(srcblk)).."."..cyan(ts(srcport)).." -> "
		 ..green(tgtblk).." (iblock)")
      else
	 -- both src and target are ports
	 local bufflen = c.buffer_length or 1

	 info("connecting "..green(ts(srcblk))..'.'..cyan(ts(srcport))
		 .." -["..yellow(ts(bufflen), true).."]".."-> "
		 ..green(ts(tgtblk)).."."..cyan(ts(tgtport)))

	 local bsrc = ubx.block_get(ni, srcblk)
	 local btgt = ubx.block_get(ni, tgtblk)

	 if bsrc==nil then
	    err_exit(1, "ERR: no block named "..srcblk.. " found")
	 end

	 if btgt==nil then
	    err_exit(1, "ERR: no block named "..tgtblk.. " found")
	 end
	 ubx.conn_lfds_cyclic(bsrc, srcport, btgt, tgtport, bufflen)
      end
   end
   mapconns(do_connect, root_sys)
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

   local ni = ubx.node_create(t.nodename, t.loglevel)

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
