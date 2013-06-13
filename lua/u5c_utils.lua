local md5 = require "md5"
local m={}
--- Convert a string to a hex representation of a string.
function m.str_to_hexstr(str,spacer)
   return (string.gsub(str,"(.)",
		       function (c)
			  return string.format("%02X%s",string.byte(c), spacer or "")
		       end ) )
end

--- Generate a hex md5sum of a string.
-- @param str string to hash
-- @return hex string
function m.md5sum_hex(str) return str_to_hexstr(md5.sum(str)) end

--- Compute the md5sum of a given struct.
-- Used to compare structs
-- TODO should operate on the body of a struct only.
--
function m.struct_md5(str)
   return m.md5sum_hex(string.gsub(str, "[%s%c]+", " "))
end

--- Read the entire contents of a file.
-- @param file name of file
-- @return string contents
function m.read_file(file)
   local f = io.open(file, "rb")
   local data = f:read("*all")
   f:close()
   return data
end


local typedef_struct_patt = "^%s*typedef%s+struct%s+([%w_]*)%s*(%b{})%s*([%w_]+)%s*;"
local struct_def_patt = "^%s*struct%s+([%w_]+)%s*(%b{});"
local def_sep = '___'

--- Extend a struct definition with a namespace
-- namespace is prepended to identifiers separated by three '_'
-- @param ns string namespace to add
-- @param struct struct as string
-- @return rewritten struct of
function m.struct_add_ns(ns, struct_str)
   local function pname(n) return ns..def_sep..n end
   local name, body, alias
   local res = ""

   -- try matching a typedef struct [name] { ... } name_t;
   -- name could be empty
   name, body, alias = string.match(struct_str, typedef_struct_patt)

   if body and alias then
      if name ~= "" then name = pname(name) end
      return "typedef struct "..name.." "..body.." "..pname(alias)..";"
   end

   -- try matching a simple struct def
   name, body = string.match(struct_str, struct_def_patt)
   if name and body then
      return "struct "..pname(name).." "..body..";"
   end

   return false
end

return m