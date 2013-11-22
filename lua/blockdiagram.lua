local umf = require "umf"
local strict = require "strict"
local utils = require "utils"

local M={}

AnySpec=umf.AnySpec
NumberSpec=umf.NumberSpec
StringSpec=umf.StringSpec
TableSpec=umf.TableSpec
ObjectSpec=umf.ObjectSpec

uoo_type = umf.uoo_type
instance_of = umf.instance_of

system = umf.class("system")

--- modules spec
modules_spec = TableSpec
{
   name='modules',
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
   name='config',
   array = { AnySpec{} },
   sealed='both'
}

--- system spec
system_spec = ObjectSpec
{
   name='system',
   type=system,
   sealed='both',
   dict={
      imports=modules_spec,
      blocks=blocks_spec,
      connections=connections_spec,
      configurations=configs_spec,
   },
}

function system:validate(verbose)
   return umf.check(self, system_spec, verbose)
end

function system:launch()
   if not self:validate(true) then return false end

   print("launching block diagram system")
   print("importing ", table.concat(self.imports), ", ")
end




-- umf.check(s1, system_spec, true)
M.system = system
M.system_spec = system_spec
return M