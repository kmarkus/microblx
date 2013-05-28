local md5 = require "md5"

--- Convert a string to a hex representation of a string.
local function str_to_hexstr(str,spacer)
   return (string.gsub(str,"(.)",
		       function (c)
			  return string.format("%02X%s",string.byte(c), spacer or "")
		       end ) )
end

--- Generate a hex md5sum of a string.
-- @param str string to hash
-- @return hex string
local function md5sum_hex(str) return str_to_hexstr(md5.sum(str)) end

--- Read the entire contents of a file.
-- @param file name of file
-- @return string contents
function read_file(file)
   local f = io.open(file, "rb")
   local data = f:read("*all")
   f:close()
   return data
end


--- Parse a C-struct.
-- @param @file
-- @return
-- md5sum, name, name-alias, structure
local function parse_struct(file)
end

return { 
   str_to_hexstr = str_to_hexstr,
   md5sum_hex    = md5sum_hex,
   read_file	 = read_file,
}