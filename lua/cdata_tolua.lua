local ffi = require "ffi"
local reflect = require "reflect"

local M={}

function M.cdata_tolua(cd, refct)
   local res

   if type(cd)~='cdata' then return cd end
   refct = refct or reflect.typeof(cd)

   local function do_struct(cd, refct)
      res = {}
      for field_refct in refct:members() do
	 res[field_refct.name]=M.cdata_tolua(cd[field_refct.name])
      end
      return res
   end

   local function is_string(refct)
      if refct.element_type.what=='int' and refct.element_type.size==1 then return true end
      return false
   end

   local function do_array(cd, refct)
      if is_string(refct) then return ffi.string(cd) end
      local res={}
      local num_elem=refct.size/refct.element_type.size
      for i=0,num_elem-1 do res[i+1]=M.cdata_tolua(cd[i]) end
      return res
   end

   if refct.what=='int' or refct.what=='float' then res=tonumber(cd)
   elseif refct.what=='struct' then res=do_struct(cd, refct)
   elseif refct.what=='array' then res=do_array(cd, refct)
   elseif refct.what=='ref' or refct.what=='ptr' then res=M.cdata_tolua(cd, refct.element_type)
   elseif refct.what=='union' then return "<union "..refct.name..">"
   else print("can't handle "..refct.what..", ignoring.") end
   return res
end

return M