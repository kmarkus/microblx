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
int=ffi.typeof("int")
char_arr=ffi.typeof("char[20]")
double_arr=ffi.typeof("double[5]")

multi_arr=ffi.typeof("int[3][3]")

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

int_inst = ffi.new(int)
char_arr_inst = ffi.new(char_arr, "frubagatschi")
multi_arr_inst=multi_arr({{1,2,3},{4,5,6},{7,8,9}})
double_arr_inst=double_arr({1.1,2.2,3.3,4.4,5.5})

point_ser=cdata.gen_logfun(point)
line_ser=cdata.gen_logfun(line)
path_ser=cdata.gen_logfun(path)
path2_ser=cdata.gen_logfun(path2)
int_ser=cdata.gen_logfun(int)
char_arr_ser=cdata.gen_logfun(char_arr)
multi_arr_ser=cdata.gen_logfun(multi_arr)
double_arr_ser=cdata.gen_logfun(double_arr)

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

int_ser("header", io.stdout); io.stdout:write("\n")
int_ser(int_inst, io.stdout); io.stdout:write("\n")

char_arr_ser("header", io.stdout); io.stdout:write("\n")
char_arr_ser(char_arr_inst, io.stdout); io.stdout:write("\n")

print(string.rep("-", 80))
print(utils.tab2str(cdata.ctype_destruct(multi_arr)))
print(utils.tab2str(cdata.flatten_keys(cdata.ctype_destruct(multi_arr))))

multi_arr_ser("header", io.stdout); io.stdout:write("\n")
multi_arr_ser(multi_arr_inst, io.stdout); io.stdout:write("\n")

double_arr_ser("header", io.stdout); io.stdout:write("\n")
double_arr_ser(double_arr_inst, io.stdout); io.stdout:write("\n")
