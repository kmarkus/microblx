--
-- A small helper module for conveniently instantiating luablocks
--

local ubx = require("ubx")
local utils = require("utils")

local M = {}

M.DEFAULT_PREFIXES = {
   "/usr/share/ubx/blocks",
   "/usr/local/share/ubx/blocks",
}

local loaded = {}

setmetatable(loaded, { __mode = "k" })

local function load_luablock(nd)
   if not loaded[nd] then
      ubx.load_module(nd, "luablock")
   end
end

--- Create a new luablock
-- @param node
-- @param block	either a) a path to file with lua block or b) a blockname (w/o extension) to search for in path
-- @param name name of the block
-- @param tgtstate optional: desired state of the block
-- @param return block
function M.create(nd, block, name, tgtstate)
   local fn, b

   load_luablock(nd)

   fn = utils.file_exists(block) and block

   if not fn then
      local ver = string.sub(ubx.safe_tostr(ubx.version()), 1, 3)

      for _, d in ipairs(M.DEFAULT_PREFIXES) do
	 local f = d .. "/" .. ver .. "/" .. block .. ".lua"
	 if utils.file_exists(f) then
	    fn = f
	    break
	 end
      end

      if not fn then
	 error("no module " .. block .. " found in as file or under " ..
	       table.concat(M.DEFAULT_PREFIXES, ', ') .. " with version " .. ver)
      end
   end

   b = ubx.block_create(nd, "ubx/luablock", name, { lua_file=fn } )

   if tgtstate then
      ubx.block_tostate(b, tgtstate)
   end

   return b
end

return M
