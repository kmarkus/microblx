ffi = require "ffi"
reflect = require "reflect"
utils=require "utils"
require"strict"

ffi.cdef[[
struct point {
   int x;
   int y;
};

struct line {
   struct point p1;
   struct point p2;
};

struct path {
   struct line l1;
   struct line l2;
   char foo[8];
};

struct person {
   char name[22];
   float age;
};
]]

point=ffi.typeof("struct point")
line=ffi.typeof("struct line")

p1 = ffi.new("struct point", { x=33, y=55 })
l1 = ffi.new("struct line", { p1= { x=3, y=5 }, p2={ x=4, y=7 }})
path1 = ffi.new("struct path", { l1={ p1= { x=3, y=5 }, p2={ x=4, y=7 }},
				 l2={ p1= { x=2222, y=5555 }, p2={ x=44444, y=7777 }},
				 foo="foobar" }
	     )

pers1 = ffi.new("struct person", { name='joe miller', age=22.3 })

arr1=ffi.new("int[10]", {0,1,2,3,4,5,6,7,8,9})
charr1=ffi.new("unsigned char[10]", "hullo")

function cdata_tolua(cd)
   
   if type(cd)~='cdata' then return cd end
   local refct = reflect.typeof(cd)
   local res

   if refct.what=='struct' then
      res = {}
      for field_refct in refct:members() do
	 res[field_refct.name]=cdata_tolua(cd[field_refct.name])
      end
   elseif refct.what=='union' then
      return "<union "..refct.name
   elseif refct.what=='ref' then
      if refct.element_type.what=='struct' then
	 -- create copy and recall ourself
	 res=cdata_tolua(ffi.new('struct '..refct.element_type.name, cd))
      elseif refct.element_type.what=='array' 
	 -- guess what might be strings
	 and refct.element_type.element_type.what=='int'
	 and refct.element_type.element_type.size==1 then
	 res=ffi.string(cd)
      elseif refct.element_type.what=='array' then
	 -- any other type of array
	 local num_elem=refct.element_type.size/refct.element_type.element_type.size
	 res={}
 	 for i=0,num_elem-1 do res[i+1]=cdata_tolua(cd[i]) end
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
 	 for i=0,num_elem-1 do res[i+1]=cdata_tolua(cd[i]) end
      end
   else
      print("can't handle "..refct.what..", ignoring.")
   end
   return res
end

-- for refct in reflect.typeof(p1):members() do print(refct.name) end

print("number:", utils.tab2str(cdata_tolua(ffi.new("int", 33))))

print("p1:", utils.tab2str(cdata_tolua(p1)))

print("l1:", utils.tab2str(cdata_tolua(l1)))

print("path1:", utils.tab2str(cdata_tolua(path1)))

print("pers1:", utils.tab2str(cdata_tolua(pers1)))

print("arr1:", utils.tab2str(cdata_tolua(arr1)))

print("charr1:", utils.tab2str(cdata_tolua(charr1)))

