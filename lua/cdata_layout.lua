cdata = require "cdata"
utils = require "utils"
ffi = require "ffi"
reflect = require "reflect"
require"strict"

cd2lua=cdata.tolua

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

struct path2 {
   struct line l[3];
   int len[3];
};

struct person {
   char name[22];
   float age;
};
]]

point=ffi.typeof("struct point")
line=ffi.typeof("struct line")
path=ffi.typeof("struct path")
path2=ffi.typeof("struct path2")

p1 = ffi.new("struct point", { x=33, y=55 })
l1 = ffi.new("struct line", { p1= { x=3, y=5 }, p2={ x=4, y=7 }})
path1 = ffi.new("struct path", { l1={ p1= { x=3, y=5 }, p2={ x=4, y=7 }},
				 l2={ p1= { x=2222, y=5555 }, p2={ x=44444, y=7777 }},
				 foo="foobar" })

path2_inst = ffi.new("struct path2", {
			l={
			   { p1={ x=3, y=5 }, p2={ x=4, y=7 }},
			   { p1={ x=33, y=55 }, p2={ x=44, y=77 }},
			   { p1={ x=333, y=555 }, p2={ x=444, y=777 }}
			},
			len = { 11,22,33} }
		    )


point_ser=cdata.gen_fast_ser(point)
line_ser=cdata.gen_fast_ser(line)
path_ser=cdata.gen_fast_ser(path)
path2_ser=cdata.gen_fast_ser(path2)

point_ser("header", io.stdout); io.stdout:write("\n")
point_ser(p1, io.stdout); io.stdout:write("\n")

line_ser("header", io.stdout); io.stdout:write("\n")
line_ser(l1, io.stdout); io.stdout:write("\n")
line_ser(l1, io.stdout); io.stdout:write("\n")
line_ser(l1, io.stdout); io.stdout:write("\n")
line_ser(l1, io.stdout); io.stdout:write("\n")

path_ser("header", io.stdout); io.stdout:write("\n")
path_ser(path1, io.stdout); io.stdout:write("\n")

path2_ser("header", io.stdout); io.stdout:write("\n")
path2_ser(path2_inst, io.stdout); io.stdout:write("\n")
