local cdata_tolua = require "cdata_tolua"
local utils = require "utils"
local ffi = require "ffi"
require"strict"

cd2lua=cdata_tolua.cdata_tolua

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

---
p2 = ffi.new("struct point", {x=66,y=99})
pptr2 = ffi.new("struct point*", p2)
p3=ffi.new("struct point")
ffi.copy(p3, pptr2, ffi.sizeof("struct point"))

pers1 = ffi.new("struct person", { name='joe miller', age=22.3 })

arr1=ffi.new("int[10]", {0,1,2,3,4,5,6,7,8,9})
charr1=ffi.new("unsigned char[10]", "hullo")


-- for refct in reflect.typeof(p1):members() do print(refct.name) end

print("number:", utils.tab2str(cd2lua(ffi.new("int", 33))))

print("p1:", utils.tab2str(cd2lua(p1)))

print("l1:", utils.tab2str(cd2lua(l1)))

print("path1:", utils.tab2str(cd2lua(path1)))

print("pers1:", utils.tab2str(cd2lua(pers1)))

print("arr1:", utils.tab2str(cd2lua(arr1)))

print("charr1:", utils.tab2str(cd2lua(charr1)))

print("p2:", utils.tab2str(cd2lua(p2)))
print("p3:", utils.tab2str(cd2lua(p3)))
print("pptr2", utils.tab2str(cd2lua(pptr2)))