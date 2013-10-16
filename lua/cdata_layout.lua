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



--- Evaluate a chunk of code in a constrained environment.
-- @param unsafe_code code string
-- @param optional environment table.
-- @return true or false depending on success
-- @return function or error message
local function eval_sandbox(unsafe_code, env)
   env = env or {}
   local unsafe_fun, msg = load(unsafe_code, nil, 't', env)
   if not unsafe_fun then return false, msg end
   return pcall(unsafe_fun)
end

--- Preprocess the given string.
-- Lines starting with # are executed as Lua code
-- Other lines are passed through verbatim, expect those contained in
-- $(...) which are executed as Lua too.
--
-- @param str string to preprocess
-- @param env environment for sandbox (default {})
-- @return preprocessed result.
function preproc(str, env)
   local chunk = {"__res={}\n" }
   local lines = utils.split(str, "\n")

   for _,line in ipairs(lines) do
      line = utils.trim(line)
      if string.find(line, "^#") then
	 chunk[#chunk+1] = string.sub(line, 2) .. "\n"
      else
	 local last = 1
	 for text, expr, index in string.gmatch(line, "(.-)$(%b())()") do
	    last = index
	    if text ~= "" then
	       -- write part before expression
	       chunk[#chunk+1] = string.format('__res[#__res+1] = %q\n ', text)
	    end
	    -- write expression
	    chunk[#chunk+1] = string.format('__res[#__res+1] = %s\n', expr)
	 end
	 -- write remainder of line (without further $()
	 chunk[#chunk+1] = string.format('__res[#__res+1] = %q\n', string.sub(line, last).."\n")
      end
   end
   chunk[#chunk+1] = "return table.concat(__res, '')\n"
   return eval_sandbox(table.concat(chunk), env)
end

--- Create a Lua version of a ctype
function refct_destruct(refct)
   local res

   local function  is_prim_num(refct)
      if refct.what=='int' or refct.what=='float' or refct.what=='void' then return true end
      return false
   end

   local function is_string(refct)
      if refct.element_type.what=='int' and refct.element_type.size==1 then return true end
      return false
   end

   local function do_struct(refct)
      res = {}
      for field_refct in refct:members() do
	 res[field_refct.name]=refct_destruct(field_refct.type)
      end
      return res
   end

   local function do_array(refct)
      if is_string(refct) then return 'string' end
      local res={}
      local num_elem=refct.size/refct.element_type.size
      for i=0,num_elem-1 do res[i]=refct_destruct(refct.element_type) end
      return res
   end

   if refct.what=='int' or refct.what=='float' then res='number'
   elseif refct.what=='struct' then res=do_struct(refct)
   elseif refct.what=='array' then res=do_array(refct)
   elseif refct.what=='ref' then res=refct_destruct(refct.element_type)
   elseif  refct.what=='ptr' then
      -- Don't touch any char*, because we don't know if they are zero
      -- terminated or not
      if cd==nil then res='NULL'
      elseif is_prim_num(refct.element_type) then res='number'
      else res=refct_destruct(refct.element_type) end
   else print("can't handle "..refct.what..", ignoring.") end

   return res
end

function ctype_destruct(ctype)
   return refct_destruct(reflect.typeof(ctype))
end

--- Flatten table and subtable keys.
-- @param t table to flatten
function flatten_keys(t)
   local function __flatten_keys(t, res, prefix)
      for k,v in pairs(t) do
	 if type(v)=='table' then
	    if prefix=="" then __flatten_keys(v, res, k)
	    else
	       if type(k)=='number' then
		  __flatten_keys(v, res, prefix..'['..k..']')
	       else
		  __flatten_keys(v, res, prefix..'.'..k)
	       end
	    end
	 else
	    if type(k)=='number' then
	       res[#res+1] = { key=prefix..'['..k..']', value=v }
	    else
	       if prefix=="" then
		  res[#res+1] = { key=k, value=v }
	       else
		  res[#res+1] = { key=prefix.."."..k, value=v }
	       end
	    end
	 end
      end
      return res
   end
   return __flatten_keys(t, {}, "")
end

---
function gen_fast_ser(ctype)
   local flattab = flatten_keys(ctype_destruct(ctype))
   table.sort(flattab, function(t1,t2) return t1.key < t2.key end)

   -- add 'serfun' field holding serialization function
   utils.foreach(function(e)
		    if e.value == 'number' then e.serfun = "tonumber"
		    elseif e.value == 'string' then e.serfun = "ffi.string"
		    else
		       error("unkown value "..e.value.." for key "..e.key)
		    end
		 end, flattab)
   local ok, res = preproc(
[[
return function(x, fd)
    if x=='header' then
    # for i=1,#flattab do
	fd:write("$(flattab[i].key)")
    #   if i<#flattab then
	  fd:write("$(separator)")
    #   end
    # end
      fd:write("\n")
      return
    end

    assert(tostring(ffi.typeof(x))=='$(tostring(ctype))',
	  "serializer: argument not a $(tostring(ctype)) but a "..tostring(ffi.typeof(x)))
    # for i=1,#flattab do
	fd:write($(flattab[i].serfun)(x$("."..flattab[i].key)))
    #   if i<#flattab then
	  fd:write("$(separator)")
    #   end
    # end
      fd:write("\n")
end
]], { io=io, table=table, ipairs=ipairs, flattab=flattab, tostring=tostring, ctype=ctype, separator=', ' })

   assert(ok, res)
   print(res)
   ok, res = eval_sandbox(res, { print=print, ffi=ffi, io=io, assert=assert,
				 tostring=tostring, tonumber=tonumber })
   assert(ok, res)
   return res
end

--    return loadstring([[
-- return function(x)

-- end
-- ]])()


-- --- Convert the given FFI ctype to a Lua table.
-- -- @param ct
-- -- @return Lua table
-- function destruct_ctype(ct)
--    local function __struct_table(rct)
--       if rct.what == 'int' then return 'int'
--       elseif rct.what == 'struct' then

--       end
--    end


--    rct = reflect.typeof(ct)
--    return __struct_table(rtc)
-- end

-- p_rct = reflect.typeof(point)
-- l_rct = reflect.typeof(line)
-- pa_rct = reflect.typeof(path)

-- print(utils.tab2str(p_rct))
-- for mem_rct in l_rct:members() do
--    print("  ", utils.tab2str(mem_rct))
-- end

-- print("----")

-- print(utils.tab2str(l_rct))

-- for mem_rct in p_rct:members() do
--    print("  ", utils.tab2str(mem_rct))
-- end


-- print("point")
-- print(utils.tab2str(ctype_destruct(point)))
-- print("line")
-- print(utils.tab2str(ctype_destruct(line)))
-- print("path")
-- print(utils.tab2str(ctype_destruct(path)))

-- local ftab = flatten_keys(ctype_destruct(path))
-- for _,v in ipairs(ftab) do
--    print(utils.tab2str(v))
-- end

-- print("sorted")

-- table.sort(ftab, function (t1,t2) return t1.key < t2.key end)

-- for _,v in ipairs(ftab) do
--    print(utils.tab2str(v))
-- end


print("------------")
point_ser=gen_fast_ser(point)
line_ser=gen_fast_ser(line)
path_ser=gen_fast_ser(path)

point_ser("header", io.stdout)
point_ser(p1, io.stdout)

line_ser("header", io.stdout)
line_ser(l1, io.stdout)
line_ser(l1, io.stdout)
line_ser(l1, io.stdout)
line_ser(l1, io.stdout)

path_ser("header", io.stdout)
path_ser(path1, io.stdout)
