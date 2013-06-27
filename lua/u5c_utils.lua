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
   return m.md5
end




return m