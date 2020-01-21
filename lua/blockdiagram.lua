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

local AnySpec=umf.AnySpec
local NumberSpec=umf.NumberSpec
local StringSpec=umf.StringSpec
local TableSpec=umf.TableSpec
local ObjectSpec=umf.ObjectSpec

local system = umf.class("system")

--- apply func to systems including all subsystems
-- @param func function(system, name, parent-system)
-- @param root root system
-- @return list of return values of func
local function mapsys(func, root)
   local res = {}

   local function __mapsys(sys,name,psys)
      res[#res+1] = func(sys, name, psys)
      for k,s in pairs(sys.subsystems or {}) do __mapsys(s, k, sys) end
   end
   __mapsys(root)
   return res
end


--- Apply func to all blocks include subsystem blocks
-- @param func function(block, block_index, parent_system)
-- @param root root system
-- @return list of return values of func
local function mapblocks(func, root)
   local res = {}

   local function __mapblocks(s)
      for i,bt in ipairs(s.blocks or {}) do
	 res[#res+1] = func(bt, i, s)
      end
   end
   mapsys(__mapblocks, root)
end

--- Apply func to all connections including subsystems
-- @param func function(conn, conn_index, parent_system)
-- @param root root system
-- @return list of return values of func
local function mapconns(func, root)
   local res = {}

   local function __mapconns(s)
      for i,ct in ipairs(s.connections or {}) do
	 res[#res+1] = func(ct, i, s)
      end
   end
   mapsys(__mapconns, root)
end

--- return the fqn of system s
local function fqn_get(s)
   local fqn = {}
   local function __fqn_get(s)
      if s._parent == nil then return
      else __fqn_get(s._parent) end
      fqn[#fqn+1] = s._name
      fqn[#fqn+1] = '/'
   end
   __fqn_get(s)
   return table.concat(fqn)
end


--- System constructor
function system:init()
   -- create system _name fields
   mapsys(
      function(s,n,p)
	 if p==nil then s._name='root' else s._name = n end
      end, self)


   -- add _parent links
   mapsys(
      function(s,n,p)
	 if p ~= nil then s._parent = p end
      end, self)

end

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
	 dict = { name=StringSpec{}, type=StringSpec{} },
	 sealed='both',
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
	 dict = { name=StringSpec{}, config=AnySpec{} },
	 sealed='both'
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

local function is_system(s)
   return umf.uoo_type(s) == 'instance' and umf.instance_of(system, s)
end

--- Validate a blockdiagram model.
function system:validate(verbose)
   return umf.check(self, system_spec, verbose)
end

--- late checks
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

--- late checks
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


-- list of late checks
M._checks = {
   unconn_inports = {
      fun=lc_unconn_inports, desc="warn about unconnected input ports"
   },
   unconn_outports = {
      fun=lc_unconn_outports, desc="warn about unconnected output ports"
   }
}

local function late_checks(self, conf, ni)
   if not conf.checks then return end
   local res = {}
   info("running late validation ("..
	   table.concat(conf.checks, ", ")..")")
   utils.foreach(
      function (chk)
	 if not M._checks[chk] then
	    err_exit(-1, "unknown check "..chk)
	 end
	 M._checks[chk].fun(ni,res)
      end,
      conf.checks or {})

   utils.foreach(function(v) warn(v) end, res)

   if #res > 0 and conf.werror then
      err_exit(-1, "warnings raised and treating warnings as errors")
   end
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
   utils.foreach(
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

--- Instantiate ubx_data for node configuration
-- @param node_config bd.configurations.node field
-- @return table of initialized config_name=ubx_data pairs
local function create_node_config(ni, node_config)
   local res = {}
   local function __create_nc(cfg, name)
      local d = ubx.data_alloc(ni, cfg.type, 1)
      ubx.data_set(d, cfg.config, true)
      res[name]=d
      info("creating node config "..blue(name)..
	     "["..magenta(cfg.type).."] "..
	     yellow(utils.tab2str(cfg.config)))
   end
   utils.foreach(__create_nc, node_config)
   return res
end


--- load the systems modules
-- @param s system
local function import_modules(ni, s)
   local loaded = {}

   mapsys(
      function(s)
	 utils.foreach(
	    function(m)
	       if loaded[m] then
		  info("skipping already loaded module "..magenta(m))
	       else
		  info("importing module "..magenta(m))
		  ubx.load_module(ni, m)
		  loaded[m]=true
	       end
	    end, s.imports)
      end, s)
end

--- Instantiate blocks
-- @param s system
local function create_blocks(ni, root_sys)
   mapblocks(
      function(b,i,p)
	 local bfqn = fqn_get(p)..b.name
	 info("creating block "..green(bfqn).." ["..blue(b.type).."]")
	 ubx.block_create(ni, b.type, bfqn)
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

   --- Preprocess configs
   -- @param ni node_info
   -- @param c configuration
   -- Substitute #blockanme with corresponding ubx_block_t ptrs
   local function preproc_configs(ni, c)
      local ret=true

      --- Substitute #blockname with ubx_block_t ptr
      local function subs_blck_ptrs(val, tab, key)
	 local name=string.match(val, ".*#([%w_-]+)")
	 if not name then return end
	 local ptr=ubx.block_get(ni, name)
	 if ptr==nil then
	    err_exit(-1, "error: failed to resolve block #"..
		     name.." for ubx_block_t* conversion")
	 elseif ubx.is_proto(ptr) then
	    err_exit("error: block #"..name.." is a proto block")
	 end
	 info(magenta("resolved block #"..name))
	 tab[key]=ptr
      end
      utils.maptree(subs_blck_ptrs, c, function(v,k) return type(v)=='string' end)

      return ret
   end

   --- Configure a the given block with a config
   -- (scalar, table or node cfg reference '&ndcfg'
   -- @param c blockdiagram.config
   -- @param b ubx_block_t
   local function apply_conf(blkconf, b)
      local function check_noderef(val)
	 if type(val) ~= 'string' then return end
	 return string.match(val, "^%s*&([%w_-]+)")
      end

      for name,val in pairs(blkconf.config) do

	 -- check for references to node configs
	 local nodecfg = check_noderef(val)

	 if nodecfg then
	    if not _NC[nodecfg] then
	       err_exit(1, "invalid node config reference '"..val.."'")
	    end
	    local blkcfg = ubx.block_config_get(b, name)

	    if blkcfg==nil then
	       err_exit(1, "non-existing config '"..blkconf.name.. "."..name.."'")
	    end

	    ubx.config_assign(blkcfg, _NC[nodecfg])
	    info("cfg "..green(blkconf.name.."."..blue(name))..
		    " -> node cfg "..yellow(nodecfg))
	 else -- regular config
	    info("cfg "..green(blkconf.name).."."..blue(name)
		    ..": "..yellow(utils.tab2str(val)))
	    ubx.set_config(b, name, val)
	 end
      end
   end

   -- Instatiate the system recursively.
   -- @param self system
   -- @param t global configuration
   -- @param ni node_info
   local function __launch(self, t, ni)
      self.include = self.include or {}
      self.imports = self.imports or {}
      self.blocks = self.blocks or {}
      self.configurations = self.configurations or {}
      self.connections = self.connections or {}
      self.start = self.start or {}

      info("launching system in node "..ts(t.nodename))

      if not preproc_configs(ni, self.configurations) then
	 err_exit(1, "preprocessing configs failed")
      end

      -- launch subsystems
      if #self.include > 0 then
	 utils.foreach(function(s) __launch(s, t, ni) end, self.include)
      end

      -- apply configuration
      if #self.configurations > 0 then
	 utils.imap(
	    function(c)
	       local b = ubx.block_get(ni, c.name)
	       if b==nil then
		  info(red("no block "..c.name.." ignoring configuration "..utils.tab2str(c.config)))
	       elseif b:get_block_state()~='preinit' then return
	       else
		  -- apply this config to and then find configs for
		  -- this block in super-systems (parent systems)
		  apply_conf(c, b)
		  local walker = self._parent
		  while walker ~= nil do
		     utils.foreach(function (cc)
				      if cc.name==c.name then apply_conf(cc, b) end
				   end, walker.configurations)
		     walker = walker._parent
		  end
		  -- configure the block
		  local ret = ubx.block_init(b)
		  if ret ~= 0 then
		     err_exit(ret, "failed to initialize block "..c.name..
				 ": "..tonumber(ret))
		  end
	       end
	    end, self.configurations)
      end

      -- create connections
      if #self.connections > 0 then
	 utils.foreach(function(c)
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
      late_checks(self, t, ni)

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

   -- _NC = build_nodecfg_tab(self)

   -- configure_blocks(ni, self, NC)

   -- connect_blocks

   -- startup blocks
   -- start_system(ni, self)


   if self.node_configurations then
      _NC = create_node_config(ni, self.node_configurations)
   end
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
