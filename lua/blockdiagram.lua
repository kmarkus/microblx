local ubx = require "ubx"
local umf = require "umf"
local strict = require "strict"
local utils = require "utils"
local ac = require "ansicolors"
local ts = tostring
local M={}

-- happy colors
color = true
local function red(str, bright) if color then str = ac.red(str); if bright then str=ac.bright(str) end end return str end
local function blue(str, bright) if color then str = ac.blue(str); if bright then str=ac.bright(str) end end return str end
local function cyan(str, bright) if color then str = ac.cyan(str); if bright then str=ac.bright(str) end end return str end
local function white(str, bright) if color then str = ac.white(str); if bright then str=ac.bright(str) end end return str end
local function green(str, bright) if color then str = ac.green(str); if bright then str=ac.bright(str) end end return str end
local function yellow(str, bright) if color then str = ac.yellow(str); if bright then str=ac.bright(str) end end return str end
local function magenta(str, bright) if color then str = ac.magenta(str); if bright then str=ac.bright(str) end end return str end

AnySpec=umf.AnySpec
NumberSpec=umf.NumberSpec
StringSpec=umf.StringSpec
TableSpec=umf.TableSpec
ObjectSpec=umf.ObjectSpec

uoo_type = umf.uoo_type
instance_of = umf.instance_of

system = umf.class("system")

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
      imports=imports_spec,
      blocks=blocks_spec,
      connections=connections_spec,
      configurations=configs_spec,
   },
   optional={'imports', 'blocks', 'connections', 'configurations' },
}

--- Validate a blockdiagram model.
function system:validate(verbose)
   return umf.check(self, system_spec, verbose)
end


--- Launch a blockdiagram system
-- @param self system specification to load
-- @param t configuration table
-- @return ni node_info handle
function system:launch(t)
   if self:validate(false) > 0 then
      self:validate(true)
      os.exit(1)
   end

   local log
   if t.verbose then log=print
   else log=function() end end

   -- Preprocess configs
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
	 end
	 log(magenta("    resolved block #"..name.." and converted to ptr "..ts(ptr)))
	 tab[key]=ptr
      end
      utils.maptree(subs_blck_ptrs, c, function(v,k) return type(v)=='string' end)
      return ret
   end

   self.imports = self.imports or {}
   self.blocks = self.blocks or {}
   self.configurations = self.configurations or {}
   self.connections = self.connections or {}

   local ni = ubx.node_create(t.nodename)

   log("launching block diagram system in node "..ts(t.nodename))

   log("importing "..ts(#self.imports).." modules... ")
   utils.foreach(function(m)
		    log("    "..magenta(m) )
		    ubx.load_module(ni, m)
		 end, self.imports)
   log("importing modules completed")

   log("instantiating "..ts(#self.blocks).." blocks... ")
   local btab = {}
   utils.foreach(function(b)
		    log("    "..green(b.name).." ["..blue(b.type).."]")
		    btab[b.name] = ubx.block_create(ni, b.type, b.name)
		 end, self.blocks)
   log("instantiating blocks completed")

   log("configuring "..ts(#self.configurations).." blocks... ")
   log("    preprocessing configs")
   if not preproc_configs(ni, self.configurations) then
      log(red("    failed"))
      os.exit(1)
   end

   utils.foreach(
      function(c)
	 if btab[c.name]==nil then
	    log("    no block named "..c.name.." ignoring configuration", utils.tab2str(c.config))
	 else
	    if type(c.config)=='string' then
	       log("    "..green(c.name).." (from file"..yellow(c.config)..")")
	    else
	       log("    "..green(c.name).." with "..yellow(utils.tab2str(c.config))..")")
	       ubx.set_config_tab(btab[c.name], c.config)
	    end
	 end
      end, self.configurations)

   log("configuring blocks completed")

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
		       ubx.conn_lfds_cyclic(bsrc, pnamesrc, btgt, pnametgt, bufflen)
		    end
		 end, self.connections)

   log("creating connections completed")

   return ni
end

-- exports
M.system = system
M.system_spec = system_spec

return M
