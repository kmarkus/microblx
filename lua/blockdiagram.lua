--
-- blockdiagram: DSL for specifying and launching microblx system
-- compositions
--
-- Copyright (C) 2013 Markus Klotzbuecher <markus.klotzbuecher@mech.kuleuven.be>
-- Copyright (C) 2014-2018 Markus Klotzbuecher <mk@mkio.de>
--
-- SPDX-License-Identifier: LGPL-2.1-or-later
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

local ind_log = -1
local ind_mul = 4
local verbose = true

local function log(first, ...)
   if verbose then
      print(string.rep(' ', ind_log*ind_mul)..tostring(first), ...)
   end
end

local function err_exit(code, msg)
   utils.stderr(red(msg))
   os.exit(code)
end

local AnySpec=umf.AnySpec
local NumberSpec=umf.NumberSpec
local StringSpec=umf.StringSpec
local TableSpec=umf.TableSpec
local ObjectSpec=umf.ObjectSpec

local system = umf.class("system")

function system:init()
   local function create_parent_links(subsys) subsys._parent=self end
   utils.foreach(create_parent_links, self.include or {})
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
   optional = { 'node' },
   sealed='both'
}

-- start
local start_spec = TableSpec
{
   name='start',
   sealed='both',
   array = { StringSpec },
}


--- system spec

local system_spec = ObjectSpec
{
   name='system',
   type=system,
   sealed='both',
   dict={
      include = TableSpec {
	 name='include', -- subsystems
	 array={ }, -- self reference added below
	 sealed='both',
      },
      imports=imports_spec,
      blocks=blocks_spec,
      connections=connections_spec,
      node_configurations=node_config_spec,
      configurations=configs_spec,
      start=start_spec,
      _parent=AnySpec{},
   },
   optional={ 'include', 'imports', 'blocks', 'connections', 'configurations', 'start', '_parent' },
}

-- add self include reference
system_spec.dict.include.array[#system_spec.dict.include.array+1] = system_spec

local function is_system(s)
   return umf.uoo_type(s) == 'instance' and umf.instance_of(system, s)
end

--- Validate a blockdiagram model.
function system:validate(verbose)
   return umf.check(self, system_spec, verbose)
end

--- read blockdiagram system file from usc or json file
-- @param fn file name of file (usc or json)
-- @return true or error msg, system model
local function load(fn)

   local function read_json()
      local f = assert(io.open(fn, "r"))
      local data = json.decode(f:read("*all"))
      return system(data)
   end

   local ext = string.match(fn, "^.+%.(.+)$")
   local suc, mod
   if ext == 'json' then
      if not has_json then
	 err_exit(1, "no cjson library found, unable to load json")
      end
      suc, mod = pcall(read_json, fn)
   elseif ext == 'usc' or ext == 'lua' then
      suc, mod = pcall(dofile, fn)
   else
      err_exit(1, "ubx_launch error: unknown extension "..tostring(ext))
   end

   if not is_system(mod) then
      err_exit(1, "failed to load "..ts(fn).."\n"..ts(mod))
   end

   return suc, mod
end

--- Pulldown a blockdiagram system
-- @param self system specification to load
-- @param t configuration table
function system.pulldown(self, ni)
   if #self.start > 0 then
      log("deactivating "..ts(#self.blocks).." blocks... ")
      -- stop the start table blocks in reverse order
      for i = #self.start, 1, -1 do
	 log("    deactivating " .. green(self.start[i]))
	 ubx.block_unload(ni, self.start[i])
      end
   end
   log("    deactivating remaining blocks")
   ubx.node_cleanup(ni)
   log("deactivating blocks completed")
end


--- Start up a system
-- @param self system specification to load
-- @param t configuration table
-- @return ni node_info handle
function system.startup(self, ni)
   log("activating "..ts(#self.blocks).." blocks... ")
   utils.foreach(
      function (bmodel)
	 -- skip the blocks in the start table
	 if utils.table_has(self.start, bmodel.name) then return end
	 local b = ubx.block_get(ni, bmodel.name)
	 log("    activating ".. green(bmodel.name))
	 ubx.block_tostate(b, 'active')
      end, self.blocks)
   -- start the start table blocks in order
   for _,trigname in ipairs(self.start) do
      local b = ubx.block_get(ni, trigname)
      log("    activating ".. green(trigname))
      ubx.block_tostate(b, 'active')
   end
   log("activating blocks completed")
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
      log("    "..blue(name)..
	     "["..magenta(cfg.type).."] "..
	     yellow(utils.tab2str(cfg.config)))
   end
   utils.foreach(__create_nc, node_config)
   return res
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

   --- Generate a list of all blockdiagram.blocks to be created
   -- @param blockdiagram.system
   -- @return list of blocks
   local function make_block_list(c)
      local res = {}
      local function __make_block_list(c)
	 utils.foreach(__make_block_list, c.include or {})
	 for _,b in ipairs(c.blocks or {}) do res[#res+1] = b end
      end
      __make_block_list(c)
      return res
   end

   --- Generate an order list of all modules to be imported
   -- @param blockdiagram.system
   -- @return list of imports
   local function make_import_list(c)
      local res = {}

      local function __make_import_list(c)
	 utils.foreach(__make_import_list, c.include or {})
	 for _,m in ipairs(c.imports) do res[#res+1] = m end
      end
      __make_import_list(c)
      return utils.table_unique(res)
   end

   -- Preprocess configs
   -- @param ni node_info
   -- @param c configuration
   -- Substitute #blockanme with corresponding ubx_block_t ptrs
   local function preproc_configs(ni, c)
      local ret=true
      local function subs_blck_ptrs(val, tab, key)
	 local name=string.match(val, ".*#([%w_-]+)")
	 if not name then return end
	 local ptr=ubx.block_get(ni, name)
	 if ptr==nil then
	    log(red("error: failed to resolve block #"..name.." for ubx_block_t* conversion"))
	    ret=false
	 elseif ubx.is_proto(ptr) then
	    log(red("error: block #"..name.." is a proto block"))
	    ret=false
	 end
	 log(magenta("    resolved block #"..name))
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
	    ubx.config_assign(blkcfg, _NC[nodecfg])
	    log("    "..green(blkconf.name.."."..blue(name))..
		   " -> node cfg "..yellow(nodecfg))
	 else -- regular config
	    log("    "..green(blkconf.name).."."..blue(name)
		   .." with "..yellow(utils.tab2str(val))..")")
	    ubx.set_config(b, name, val)
	 end
      end
   end

   -- Instatiate the system recursively.
   -- @param self system
   -- @param t global configuration
   -- @param ni node_info
   local function __launch(self, t, ni)
      ind_log=ind_log+1

      self.include = self.include or {}
      self.imports = self.imports or {}
      self.blocks = self.blocks or {}
      self.configurations = self.configurations or {}
      self.connections = self.connections or {}
      self.start = self.start or {}

      log("launching block diagram system in node "..ts(t.nodename))

      log("    preprocessing configs")
      if not preproc_configs(ni, self.configurations) then
	 err_exit(1, "    failed")
      end

      -- launch subsystems
      if #self.include > 0 then
	 log("loading used subsystems")
	 utils.foreach(function(s) __launch(s, t, ni) end, self.include)
	 log("loading subsystems completed")
      end

      -- apply configuration
      if #self.configurations > 0 then
	 log("configuring "..ts(#self.configurations).." blocks... ")

	 utils.imap(
	    function(c)
	       local b = ubx.block_get(ni, c.name)
	       if b==nil then
		  log(red("    no block named "..c.name..
			     " ignoring configuration "..utils.tab2str(c.config)))
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
		  ubx.block_init(b)
	       end
	    end, self.configurations)
	 log("configuring blocks completed")
      end

      -- create connections
      if #self.connections > 0 then
	 log("creating "..ts(#self.connections).." connections... ")
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
			     log("    "..green(bnamesrc).." (iblock) -> "
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
			     log("    "..green(ts(bnamesrc)).."."..cyan(ts(pnamesrc)).." -> "..green(bnametgt).." (iblock)")
			  else
			     -- standard connection between two cblocks
			     local bufflen = c.buffer_length or 1

			     log("    "..green(ts(bnamesrc))..'.'..cyan(ts(pnamesrc))
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

	 log("creating connections completed")
      end

      -- startup
      if not t.nostart then system.startup(self, ni) end

      ind_log=ind_log-1
   end

   -- fire it up
   t = t or {}
   t.nodename = t.nodename or "node-"..os.date("%Y%m%d_%H%M%S")
   verbose = t.verbose

   local ni = ubx.node_create(t.nodename)

   -- import modules
   local imports = make_import_list(self)
   log("importing "..ts(#imports).." modules... ")
   utils.foreach(function(m)
		    log("    "..magenta(m) )
		    ubx.load_module(ni, m)
		 end, imports)
   log("importing modules completed")

   --- create blocks
   local blocks = make_block_list(self)
   log("instantiating "..ts(#blocks).." blocks... ")
   utils.foreach(function(b)
		    log("    "..green(b.name).." ["..blue(b.type).."]")
		    ubx.block_create(ni, b.type, b.name)
		 end, blocks)
   log("instantiating blocks completed")

   if self.node_configurations then
      log("creating node configurations...")
      _NC = create_node_config(ni, self.node_configurations)
      log("creating node configurations completed")
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
