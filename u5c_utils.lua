local md5sum = require "md5sum"


local function str_to_hexstr(str,spacer)
   return (string.gsub(str,"(.)",
		       function (c)
			  return string.format("%02X%s",string.byte(c), spacer or "")
		       end ) )
end


local function md5sum_hex(str)
   return str_to_hexstr(md5.sum(str))
end


--- Parse a C-struct.
-- @param @file
-- @return
-- md5sum, name, name-alias, structure
local function parse_struct(file)
end

return { 
   str_to_hexstr = str_to_hexstr,
   md5sum_hex    = md5sum_hex
}