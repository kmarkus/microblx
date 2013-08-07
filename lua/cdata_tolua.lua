local ffi = require "ffi"
local reflect = require "reflect"

local M={}

function M.cdata_tolua(cd)
   
   if type(cd)~='cdata' then return cd end
   local refct = reflect.typeof(cd)
   local res

   if refct.what=='struct' then
      res = {}
      for field_refct in refct:members() do
	 res[field_refct.name]=M.cdata_tolua(cd[field_refct.name])
      end
   elseif refct.what=='union' then
      return "<union "..refct.name..">"
   elseif refct.what=='ref' then
      if refct.element_type.what=='struct' then
	 -- create copy and recall ourself
	 res=M.cdata_tolua(ffi.new('struct '..refct.element_type.name, cd))
      elseif refct.element_type.what=='array' 
	 -- guess what might be strings
	 and refct.element_type.element_type.what=='int'
	 and refct.element_type.element_type.size==1 then
	 res=ffi.string(cd)
      elseif refct.element_type.what=='array' then
	 -- any other type of array
	 local num_elem=refct.element_type.size/refct.element_type.element_type.size
	 res={}
 	 for i=0,num_elem-1 do res[i+1]=M.cdata_tolua(cd[i]) end
      end
   elseif refct.what=='ptr' then
      -- We can't reuse the 'ref' code above for this, because it is
      -- apparently not legal to initalize a struct from a
      -- pointer. Hence, we create the copy and use ffi.copy to copy
      -- the from the original
      if refct.element_type.what=='struct' then
	 local ct = ffi.typeof('struct '..refct.element_type.name)
	 local copy=ct()
	 ffi.copy(copy, cd, ffi.sizeof(ct))
	 res=M.cdata_tolua(copy)
      end
   elseif refct.what=='int' or refct.what=='float' then
      res=tonumber(cd)
   elseif refct.what=='array' then
      if refct.element_type.what=='int' and 
	 refct.element_type.size==1 then
	 res=ffi.string(cd)
      else
	 local num_elem=refct.size / refct.element_type.size
	 res={}	
 	 for i=0,num_elem-1 do res[i+1]=M.cdata_tolua(cd[i]) end
      end
   else
      print("can't handle "..refct.what..", ignoring.")
   end
   return res
end

return M