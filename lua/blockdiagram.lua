local ubx = require "ubx"
local umf = require "umf"
local strict = require "strict"
local utils = require "utils"
local ubx_env = require "ubx_env"
local ac = require "ansicolors"
local ts = tostring
local M={}
local strict = require "strict"

-- color handling via ubx
red=ubx.red; blue=ubx.blue; cyan=ubx.cyan; white=ubx.cyan; green=ubx.green; yellow=ubx.yellow; magenta=ubx.magenta

AnySpec=umf.AnySpec
NumberSpec=umf.NumberSpec
StringSpec=umf.StringSpec
TableSpec=umf.TableSpec
ObjectSpec=umf.ObjectSpec

uoo_type = umf.uoo_type
instance_of = umf.instance_of

system = umf.class("system")

function system:init()
   local function create_parent_links(subsys) subsys._parent=self end
   utils.foreach(create_parent_links, self.include or {})
end

--- imports spec
imports_spec = TableSpec
{
   name='imports',
   array = { StringSpec{} },
   postcheck=function(self, obj, vres)
      -- extra checks
      return true
   end
}

-- blocks
blocks_spec = TableSpec
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
connections_spec = TableSpec
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

-- configuration
configs_spec = TableSpec
{
   name='configurations',
   array = {
      TableSpec
      {
	 name='configuration',
	 dict = { name=StringSpec{}, config=AnySpec{} },
      },
      sealed='both'
   }
}

--- system spec

system_spec = ObjectSpec
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
      configurations=configs_spec,
      _parent=AnySpec{},
   },
   optional={ 'include', 'imports', 'blocks', 'connections', 'configurations', '_parent' },
}

-- add self include reference
system_spec.dict.include.array[#system_spec.dict.include.array+1] = system_spec

function is_system(s)
   return umf.uoo_type(s) == 'instance' and umf.instance_of(system, s)
end

--- Validate a blockdiagram model.
function system:validate(verbose)
   return umf.check(self, system_spec, verbose)
end

--- Load system from file.
-- @param file name of file
-- @return system
function load(file)
   local sys = dofile(file)
   if not is_system(sys) then
      error("blockdiagram.load: no valid system in file '" .. tostring(file) .. "' found.")
   end
   return sys
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

   local log
   local ind_log = -1
   local ind_mul = 4
   if t.verbose then
      log=function(first, ...)
	 print(string.rep(' ', ind_log*ind_mul)..tostring(first), ...)
      end
   else log=function() end end

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
   -- @reteurn list of imports
   local function make_import_list(c)
      local res = {}
      
      local function __make_import_list(c)
         local ubx_root = ubx_env.get_ubx_root()
         local ubx_modules = ubx_env.get_ubx_modules()
         if ubx_root == "" then
           log(yellow("  Warning, UBX_ROOT not defined"))
         end
         if ubx_modules == "" then
           log(yellow("  Warning, UBX_MODULES not defined"))
         end
	 utils.foreach(__make_import_list, c.include or {})
	 for _,m in ipairs(c.imports) do
           if utils.file_exists(m) then
             res[#res+1] = m 
           elseif utils.file_exists(ubx_root..m) then
             res[#res+1] = ubx_root..m
           elseif utils.file_exists(ubx_modules..m) then
             res[#res+1] = ubx_modules..m
           else
             log(red("  Module "..m.." not found! It will not be loaded"))
             log(yellow("    Tip: did you set UBX_ROOT or UBX_MODULES?"))
           end
         end
      end
      
      __make_import_list(c)
      return utils.table_unique(res)
   end

   -- Preprocess configs
   -- @param ni node_info
   -- @param c configuration
   -- Substitue #blockanme with corresponding ubx_block_t ptrs
   local function preproc_configs(ni, c)
      local ret=true
      local function subs_blck_ptrs(val, tab, key)
	 local name=string.match(val, ".*#([%w_]+)")
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

   --- Configure a the given block with a config (file or table)
   -- @param c blockdiagram.config
   -- @param b ubx_block_t
   local function apply_conf(c, b)
      if type(c.config)=='string' then
	 log("    "..green(c.name).." (from file"..yellow(c.config)..")")
      else
	 log("    "..green(c.name).." with "..yellow(utils.tab2str(c.config))..")")
	 ubx.set_config_tab(b, c.config)
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

      log("launching block diagram system in node "..ts(t.nodename))

      log("    preprocessing configs")
      if not preproc_configs(ni, self.configurations) then
	 log(red("    failed"))
	 os.exit(1)
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

	 utils.foreach(
	    function(c)
	       local b = ubx.block_get(ni, c.name)
	       if b==nil then
		  log(red("    no block named "..c.name.." ignoring configuration "..utils.tab2str(c.config)))
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

			     if ib==nil then  log(red("unkown block "..bnamesrc)); os.exit(1) end
			     if not ubx.is_iblock_instance(ib) then
				log(red(ts(bnamesrc).." not a valid iblock instance"))
				os.exit(1)
			     end

			     local btgt = ubx.block_get(ni, bnametgt)
			     local ptgt = ubx.port_get(btgt, pnametgt)

			     if ubx.port_connect_in(ptgt, ib) ~= 0 then
				log(red("failed to connect interaction "..bnamesrc..
					   " to port "..ts(bnametgt).."."..ts(pnametgt)))
			     end
			     log("    "..green(bnamesrc).." (iblock) -> "..green(ts(bnametgt)).."."..cyan(ts(pnametgt)))

			  elseif pnametgt==nil then
			     -- src="block.port", tgt="iblock"
			     local ib = ubx.block_get(ni, bnametgt)

			     if ib==nil then log(red("unkown block "..ts(bnametgt))); os.exit(1) end

			     if not ubx.is_iblock_instance(ib) then
				log(red(ts(bnametgt).." not a valid iblock instance"))
				os.exit(1)
			     end

			     local bsrc = ubx.block_get(ni, bnamesrc)
			     local psrc = ubx.port_get(bsrc, pnamesrc)

			     if ubx.port_connect_out(psrc, ib) ~= 0 then
				log(red("failed to connect "..ts(bnamesrc).."."..ts(pnamesrc)..
					   " to "..ts(bnametgt)).." (iblock)")
			     end
			     log("    "..green(ts(bnamesrc)).."."..cyan(ts(pnamesrc)).." -> "..green(bnametgt).." (iblock)")
			  else
			     -- standard connection between two cblocks
			     local bufflen = c.buffer_length or 1
			     log("    "..green(ts(bnamesrc))..'.'..cyan(ts(pnamesrc)).." -["..
				    yellow(ts(bufflen), true).."]".."-> "..green(ts(bnametgt)).."."..cyan(ts(pnametgt)))
			     local bsrc = ubx.block_get(ni, bnamesrc)
			     local btgt = ubx.block_get(ni, bnametgt)
			     if bsrc==nil then log(red("ERR: no block named "..bnamesrc.. " found")); return end
			     if btgt==nil then log(red("ERR: no block named "..bnametgt.. " found")); return end
			     ubx.conn_lfds_cyclic(bsrc, pnamesrc, btgt, pnametgt, bufflen)
			  end
		       end, self.connections)

	 log("creating connections completed")
      end
      ind_log=ind_log-1
   end

   -- fire it up
   local ni = ubx.node_create(t.nodename)

   -- Environment printout
   log("Environment setup...\n    "..green("UBX_ROOT: ")..cyan(os.getenv("UBX_ROOT")))
   log("    "..green("UBX_MODULES: ")..cyan(os.getenv("UBX_MODULES")))
   
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
