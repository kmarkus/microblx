--
-- This file is part of uMF.
--
-- (C) 2012-2017 Markus Klotzbuecher, mk@marumbi.de
--
-- You may redistribute this software and/or modify it under either
-- the terms of the GNU Lesser General Public License version 2.1
-- (LGPLv2.1 <http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html>)
-- or (at your discretion) of the Modified BSD License: Redistribution
-- and use in source and binary forms, with or without modification,
-- are permitted provided that the following conditions are met:
--    1. Redistributions of source code must retain the above copyright
--       notice, this list of conditions and the following disclaimer.
--    2. Redistributions in binary form must reproduce the above
--       copyright notice, this list of conditions and the following
--       disclaimer in the documentation and/or other materials provided
--       with the distribution.
--    3. The name of the author may not be used to endorse or promote
--       products derived from this software without specific prior
--       written permission.
--
-- THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
-- OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
-- WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
-- ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
-- DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
-- DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
-- GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
-- WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
-- NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
-- SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
--

--- micro-OO: classes, objects, simple inheritance, constructors.
-- Just enough for umf modeling.

local color=true
local logmsgs = true

local ac=require("ansicolors")
local utils=require("utils")
local ts = tostring
-- local strict = require("strict")

module("umf", package.seeall)

-- log stuff
local ind, indmul, indchar = 0, 4, ' '
function ind_inc() ind=ind+1 end
function ind_dec() ind=ind-1 end

-- Enable logging by uncomenting the following line and commenting the
-- subsequent:
--function log(...) io.write(string.rep(indchar, ind*indmul)); print(...) end
function log(...) return end

--- microObjects, heavily inspired by middleclass.
local function __class(name, super)   local klass = { name=name, superclass=super, static = {}, iops={}, __class_identifier=true }
   local iops = klass.iops
   iops.__index = iops

   if super then
      local superStatic = super.static
      setmetatable(iops, super.iops)
      setmetatable(klass.static, { __index = function(_,k) return iops[k] or superStatic[k] end })
   else
      setmetatable(klass.static, { __index = function(_,k) return iops[k] end })
   end

  setmetatable(klass, {
    __tostring = function() return "class " .. klass.name end,
    __index    = klass.static,
    __newindex = klass.iops,
    __call     = function(self, ...) return self:new(...) end
  })

  klass.class=function(_) return klass end -- enable access to class
  return klass
end

Object = __class("Object")
function class(name, super) return __class(name, super or Object) end

function Object.static:new(t)
   local obj = setmetatable(t or {}, self.iops)
   obj:init()
   return obj
end

function Object.static:super() return self.superclass end
function Object.static:classname() return self.name end
function Object.static:type() return 'class' end
function Object:init() return end

--- Return the uoo type ('class' or 'instance')
function uoo_type(x)
   if not x then return false end
   local mt = getmetatable(x)
   if mt and mt.__index==x.static then return 'class'
   elseif mt and mt.class then return 'instance' end
   return false
end

--- Return the class name of the given object.
function uoo_class(o)
   if uoo_type(o) == 'instance' then
      return o:class().name
   end
   return false
end

function subclass_of(subklass, klass)
   function sc_of(sc, k)
      if not sc then return false end
      if sc==k then return true end
      return sc_of(sc:super(), k)
   end
   assert(uoo_type(subklass)=='class', "subclass_of: first argument not a class")
   assert(uoo_type(klass)=='class', "subclass_of: second argument not a class")
   return sc_of(subklass, klass)
end

function instance_of(klass, obj)
   assert(uoo_type(klass)=='class', "instance_of: first argument not a class")
   assert(uoo_type(obj)=='instance', "instance_of: second argument not an instance")
   return subclass_of(obj:class(), klass)
end

--- uMF spec validation.

--- Split a table in array and dictionary part.
function table_split(t)
   local arr,dict = {},{}
   for i,v in ipairs(t) do arr[i] = v end
   for k,v in pairs(t) do if arr[k]==nil then dict[k]=v end end
   return arr, dict
end

-- Specifications
Spec=class("Spec")

--- Check if object complies to spec.
-- @param self spec object
-- @param obj object to validate
-- @param vres validation result structure (optional)
-- @return boolean return value meaning success
function Spec.check(self, obj, vres)
   if not instance_of(Spec, obj) then
      add_msg(vres, "err", tostring(obj).." not an instance of "..tostring(self.type))
      return false
   end
   return true
end

--- Check if obj is of the correct type.
-- This does not mean it is validated.
function Spec.is_a(self, obj, vres) error("Spec:is_a invoked") end

AnySpec=class("AnySpec", Spec)
NumberSpec=class("NumberSpec", Spec)
StringSpec=class("StringSpec", Spec)
BoolSpec=class("BoolSpec", Spec)
FunctionSpec=class("FunctionSpec", Spec)
EnumSpec=class("EnumSpec", Spec)
TableSpec=class("TableSpec", Spec)
ClassSpec=class("ClassSpec", TableSpec)
ObjectSpec=class("ObjectSpec", TableSpec)

-- concatenate the validation context in a smart way, ie. no dots
-- before [].
local function concat_ctx(ctx,sep)
   local ret = ""
   for i,v in ipairs(ctx) do
      if string.match(v, "%[(%d+)]") then ret=ret..v else ret=ret..sep..v end
   end
   return ret
end

--- Add an error to the validation result struct.
-- @param validation structure
-- @param level: 'err', 'warn', 'inf'
-- @param msg string message
function add_msg(vres, level, msg)
   local function colorize(tag, msg, bright)
      if not color then return msg end
      if tag=='inf' then msg = ac.blue(msg)
      elseif tag=='warn' then msg = ac.yellow(msg)
      elseif tag=='err' then msg = ac.red(msg)
      elseif tag=='ctx' then msg = ac.cyan(msg) end
      if bright then msg = ac.bright(msg) end
      return msg
   end

   if not vres then return end
   if not (level=='err' or level=='warn' or level=='inf') then
      error("add_msg: invalid level: " .. tostring(level))
   end
   local msgs = vres.msgs
   msgs[#msgs+1] = colorize(level, level.." @ ", true) .. colorize("ctx", concat_ctx(vres.context, '.')..": ").. colorize(level, msg)
   vres[level] = vres[level] + 1
   return vres
end

function vres_add_newline(vres)
   if not vres then return end -- not sure this is necessary/correct/
   local msgs = vres.msgs
   msgs[#msgs+1] = ''
end

--- Push error message context.
-- @param vres validation result table
-- @param field current context in form of table field.
function vres_push_context(vres, field)
   if not vres then return end
   vres.context=vres.context or {}
   if type(field) ~= 'string' then error("vres_push_context: field not a string but "..type(field)) end
   local depth = #vres.context
   vres.context[depth+1] = field
   return depth
end

--- Pop error message context.
-- @param vres validation result table
function vres_pop_context(vres, old_depth)
   if not vres then return end
   vres.context=vres.context or {}
   local cur_depth = #vres.context
   if old_depth~=nil and old_depth ~= cur_depth - 1 then
      error("vres_pop_context: unbalanced context stack at "..table.concat(vres.context, '.')..
	 ". should be "..ts(old_depth).." but is "..ts(cur_depth-1))
   end
   if #vres.context <= 0 then error("vres_pop_context with empty stack") end
   vres.context[#vres.context] = nil
end

--- True as long it is not nil.
function AnySpec.check(self, obj, vres)
   log("checking obj '"..tostring(obj).."' against AnySpec")
   if obj==nil then
      add_msg(vres, "err", "obj is nil")
      return false
   end
   return true
end

--- Number spec.
function NumberSpec.is_a(self, obj)
   return type(obj) == "number"
end

function NumberSpec.check(self, obj, vres)
   log("checking obj '"..tostring(obj).."' against NumberSpec")
   local t = type(obj)
   if t ~= "number" then
      add_msg(vres, "err", "not a number but a " ..t)
      return false
   end
   if self.min and obj < self.min then
      add_msg(vres, "err", "number value="..tostring(obj).. " beneath min="..tostring(self.min))
      return false
   end
   if self.max and obj > self.max then
      add_msg(vres, "err", "number value="..tostring(obj).. " above max="..tostring(self.max))
      return false
   end
   return true
end

--- Validate a string spec.
function StringSpec.check(self, obj, vres)
   log("checking obj '"..tostring(obj).."' against StringSpec")

   local t = type(obj)

   if t ~= "string" then
      add_msg(vres, "err", "not a string but a " ..t)
      return false
   end

   if self.regexp then
      local res = string.match(obj, self.regexp)
      if not res then
	 add_msg(vres, "err", "regexp "..tostring(self.regexp).. " didn't match string "..tostring(obj))
	 return false
      end
   end
   return true
end

--- Validate a boolean spec.
function BoolSpec.check(self, obj, vres)
   log("checking obj '"..ts(obj).."' against BooleanSpec")
   local t = type(obj)
   if t == "boolean" then return true end
   add_msg(vres, "err", "not a boolean but a " ..t)
   return false
end

--- Validate a function spec.
function FunctionSpec.check(self, obj, vres)
   log("checking obj '"..tostring(obj).."' against FunctionSpec")
   local t = type(obj)
   if t == "function" then return true end
   add_msg(vres, "err", "not a function but a " ..t)
   return false
end

--- Validate an enum spec.
function EnumSpec.check(self, obj, vres)
   log("checking obj '"..tostring(obj).."' against EnumSpec")
   if utils.table_has(self, obj) then return true end
   add_msg(vres, "err", "invalid enum value: " .. tostring(obj) .. " (valid: " .. table.concat(self, ", ")..")")
end

function TableSpec:init()
   self.dict = self.dict or {}
   self.array = self.array or {}
end

--- Validate a table spec.
-- This is the most important function of uMF.
function TableSpec.check(self, obj, vres)
   local ret=true

   local function is_a_valid_spec(entry, spec_tab, vres)
      ind_inc()
      spec_tab = spec_tab or {}
      -- if entry is an Object and we have a matching ObjectSpec then
      -- prefer that!
      if uoo_type(entry) == 'instance' then
	 for _,spec in ipairs(spec_tab) do
	    if instance_of(ObjectSpec, spec) and instance_of(spec.type, entry) then
	       log("is_a_valid_spec: strongly matched type "..spec.type.name)
	       ind_dec()
	       return spec:check(entry, vres)
	    end
	 end
      end
      ind_dec()
      -- brute force
      for _,spec in ipairs(spec_tab) do
	 if spec:check(entry, vres) then return true end
      end
      return false
   end


   --- Check if entry is a legal array entry type.
   local function check_array_entry(entry,i)
      ind_inc()
      local depth = vres_push_context(vres, "["..ts(i).."]")
      log("check_array_entry: IN  #"..ts(i).." '"..ts(entry).."'")

      local sealed = self.sealed == 'both' or self.sealed=='array'
      local arr_spec = self.array or {}

      -- if entry is valid, there is nothing else to do:
      if is_a_valid_spec(entry, arr_spec) then
	 log("check_array_entry: OUT #"..ts(i).." Ok!")
	 vres_pop_context(vres, depth)
	 ind_dec();
	 return
      end

      -- Invalid or illegal
      if sealed then
	 log("check_array_entry: #"..ts(i).." FAILED! (illegal/invalid), because:")
	 add_msg(vres, "err", "illegal/invalid array entry #"..ts(i).." '".. ts(entry).."'. Error(s) follow:")
	 is_a_valid_spec(entry, arr_spec, vres)
	 -- vres_add_newline(vres)
	 log("check_array_entry: OUT #"..ts(i).." FAILED")
	 ret=false
      else
	 log("check_array_entry: OUT #"..ts(i).." FAILED! (but unsealed)")
	 add_msg(vres, "inf", "unkown entry '"..tostring(entry) .."' in array part")
      end
      ind_dec()
      vres_pop_context(vres, depth)
   end

   --- Check if key=entry are valid for the TableSpec 'self'.
   local function check_dict_entry(entry, key)
      -- if key=='__other' then return end ?
      ind_inc()
      log("check_dict_entry: "..ts(key).."='"..ts(entry).."'")
      local depth = vres_push_context(vres, key)
      local sealed = self.sealed == 'both' or self.sealed=='dict'

      -- known key, check it.
      if self.dict[key] then
	 if not self.dict[key]:check(entry, vres) then
	    ret=false
	    log("key '" .. ts(key) .. "' found but spec checking failed")
	 else
	    log("key '" .. ts(key) .. "' found and spec checking OK")
	 end
      elseif not self.dict.__other and sealed then
	 -- unkown key, no __other and sealed -> err!
	 add_msg(vres, "err", "illegal field '"..key.."' in sealed dict (value: "..tostring(entry)..")")
	 ret=false
      elseif not self.dict[key] and is_a_valid_spec(entry, self.dict.__other) then
	 -- unknown key, but __other legitimizes entry
	 log("found matching spec in __other table")
      elseif not self.dict[key] and not is_a_valid_spec(entry, self.dict.__other) then
	 -- unkown key AND __other does not legitimze: if unsealed ->
	 -- fine. If sealed, report the errors of all checks.
	 if sealed then
	    -- gadd_msg(vres, "err", "checking __other failed for undeclared key '".. key.."' in sealed dict. Error(s) follow:")
	    -- add errmsg of __other check
	    is_a_valid_spec(entry, self.dict.__other, vres)
	    -- vres_add_newline(vres)
	    log("checking __other for key "..key.. " failed")
	    ret=false
	 else
	    add_msg(vres, "inf", "ignoring unkown field "..key.." in unsealed dict")
	 end
      else error("should not get here") end
      vres_pop_context(vres, depth)
      ind_dec()
      return
   end

   -- Check that all non optional dict entries are there
   local function check_dict_optionals(dct)
      -- build a list of non-optionals
      ind_inc()
      nopts={}
      local optional=self.optional or {}
      for field,spec in pairs(self.dict) do
	 if field~='__other' and not utils.table_has(optional, field) then
	    nopts[#nopts+1] = field
	 end
      end

      -- check all non optionals are defined
      for _,nopt_field in ipairs(nopts) do
	 if dct[nopt_field]==nil then
	    add_msg(vres, "err", "non-optional field '"..nopt_field.."' missing")
	    ret=false
	 end
      end
      ind_dec()
   end

   ind_inc()
   log("checking table spec "..(self.name or "unnamed"))

   -- check we have a table
   local t = type(obj)
   if t ~= "table" then
      add_msg(vres, "err", "not a table but a " ..t)
      ind_dec()
      return false -- fatal.
   end

   local name = self.name or "unnamed"
   -- local depth = vres_push_context(vres, name)
   if self.precheck then ret=self.precheck(self, obj, vres) end -- precheck hook

   if ret then
      local arr,dct = table_split(obj)
      utils.foreach(function (e,i) check_array_entry(e,i) end, arr)
      utils.foreach(function (e,k) check_dict_entry(e, k) end, dct)
      check_dict_optionals(dct)
   end

   if self.postcheck and ret then ret=self.postcheck(self, obj, vres) end

   log("checked table spec "..name..", result: "..tostring(ret))
   -- vres_pop_context(vres, depth)
   ind_dec()
   return ret
end

--- Check a class spec.
function ClassSpec.check(self, c, vres)
   ind_inc()
   log("validating class spec of type " .. self.name)
   local depth = vres_push_context(vres, self.name)

   -- check that they are classes
   if not uoo_type(c)=='class' or not subclass_of(c, self.type) then
      add_msg(vres, "err", "'"..tostring(c) .."' not of (sub-)class '"..tostring(self.type).."'")
      return false
   end
   vres_pop_context(vres, depth)
   local res=TableSpec.check(self, c, vres)
   ind_dec()
   return res
end

--- Check an object spec.
function ObjectSpec.check(self, obj, vres)
   local res=true
   ind_inc()
   log("validating object spec of type " .. self.name)
   -- local depth = vres_push_context(vres, self.name)
   if uoo_type(self.type) ~= 'class' then
      add_msg(vres, "err", "type field of ObjectSpec "..tostring(self.name).. " is not a umf class")
      res=false
   elseif uoo_type(obj) ~= 'instance' then
      add_msg(vres, "err", "given object is not an umf class instance but a '"..tostring(type(obj)).."'")
      res = false
   elseif not instance_of(self.type, obj) then
      add_msg(vres, "err", "'".. tostring(obj) .."' not an instance of '"..tostring(self.type).."'")
      res=false
   end
   -- vres_pop_context(vres, depth)
   if res then res=TableSpec.check(self, obj, vres) end
   log("validated object spec of type " ..self.name..": "..ts(res))
   ind_dec()
   return res
end

--- Resolve string links to objects
-- Format: "<class_name>:<object_name>
-- @param root object
-- @param verbose (optional)
-- @return false if link resolving failed, true otherwise
function resolve_links(obj, verbose)
   local failures = 0
   function __resolve_links(v,k)
      for kk,vv in pairs(v) do
	 if type(vv) == 'string' then
	    -- check if this is a link
	    local klass, objid = string.match(vv, "(%g+)#(%g+)")
	    if klass then
	       -- find the target obj
	       local no_dups = { }
	       local tgts = utils.maptree(
		  function (v,k)
		     if no_dups[v] then return end
		     no_dups[v]=true
		     return { k=k, v=v }
		  end, obj,
		  function(v) return uoo_class(v)==klass and v.name==objid end)
	       if #tgts == 0 then
		  print(ac.red("resolve_links: failed to resolve " .. ac.bright(ts(klass).."#"..ts(objid))))
		  failures = failures + 1
	       elseif #tgts > 1 then
		  print(ac.red("resolve_links: multiple targets found for ".. ac.bright(ts(klass).."#"..ts(objid))))
		  print(utils.tab2str(tgts))
		  failures = failures + 1
	       else
		  if verbose then print("resolve_links: successfully resolved target "  .. ts(klass).."#"..ts(objid)) end
		  v[kk] = tgts[1].v
	       end
	    end
	 end
      end
   end

   utils.maptree( __resolve_links, obj, function (v) return type(v) == 'table' end)
   return (failures == 0)
end


--- Print the validation results.
function print_vres(vres)
   utils.foreach(function(mes) print(mes) end, vres.msgs)
   print(tostring(vres.err) .. " errors, " .. tostring(vres.warn) .. " warnings, ".. tostring(vres.inf) .. " informational messages.")
end

--- Check a specification against an object.
-- @param object object to check
-- @param spec umf spec to check against.
-- @return number of errors
-- @return validation result table
function check(obj, spec, verb)
   -- spec must be an instance of Spec:
   log("check: IN")
   if verb then print("checking spec "..(spec.name or "unnamed")) end
   local ok, ret = pcall(instance_of, Spec, spec)
   if not ok then print("err: second argument not an Object (should be Spec instance)"); return false
   elseif ok and not ret then print("err: spec not an instance of umf.Spec\n"); return false end

   local vres = { msgs={}, err=0, warn=0, inf=0, context={} }
   spec:check(obj, vres)
   log("check: OUT")
   if verb then print_vres(vres) end
   return vres.err, vres
end
