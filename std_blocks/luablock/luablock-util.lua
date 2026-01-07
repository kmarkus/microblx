--
-- A small helper module for conveniently instantiating luablocks
--

local ubx = require("ubx")
local utils = require("utils")

local M = {}

M.DEFAULT_PREFIX = "/usr/local/share/ubx/blocks"

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
      fn = M.DEFAULT_PREFIX .. "/" .. ver .. "/" .. block .. ".lua"

      if not utils.file_exists(fn) then
	 error("no module " .. block .. " found in as file or under " .. M.DEFAULT_PREFIX .. "/" .. ver)
      end
   end

   b = ubx.block_create(nd, "ubx/luablock", name, { lua_file=fn } )

   if tgtstate then
      ubx.block_tostate(b, tgtstate)
   end

   return b
end

return M
