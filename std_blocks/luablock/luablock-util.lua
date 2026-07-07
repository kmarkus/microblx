--
-- A small helper module for conveniently instantiating luablocks
--

local ubx = require("ubx")
local utils = require("utils")

local M = {}

-- Hardcoded fallback block directories, searched after the UBX_PATH
-- derived ones (see M.block_search_dirs).
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

--- Ordered list of directories searched for luablock files.
-- The UBX_PATH-derived prefixes (`<prefix>/share/ubx/blocks`, the same
-- prefixes core modules are loaded from) come first, so a staged or
-- custom install is honored; the hardcoded M.DEFAULT_PREFIXES follow as
-- a fallback. Duplicates are removed, order preserved.
-- @return array of directory paths
function M.block_search_dirs()
   local dirs, seen = {}, {}
   local function add(d)
      if d and not seen[d] then seen[d] = true; dirs[#dirs+1] = d end
   end

   -- ubx.get_prefix() returns (core_prefix, prefixes)
   local ok, _, prefixes = pcall(ubx.get_prefix)
   if ok and type(prefixes) == "table" then
      for _, p in ipairs(prefixes) do add(p .. "/share/ubx/blocks") end
   end

   for _, d in ipairs(M.DEFAULT_PREFIXES) do add(d) end
   return dirs
end

--- Create a new luablock
-- @param nd node
-- @param block	either a) a path to file with lua block or b) a blockname (w/o extension) to search for in path
-- @param name name of the block
-- @param tgtstate optional: desired state of the block
-- @param active optional: if true or a table {period=N}, make the block
--   self-triggering using the luablock's built-in thread. When true,
--   the default period is 100 ms. When a table, the 'period' field
--   sets the period in milliseconds.
-- @return block
function M.create(nd, block, name, tgtstate, active)
   local fn, b

   load_luablock(nd)

   fn = utils.file_exists(block) and block

   if not fn then
      local ver = ubx.mod_version()
      local dirs = M.block_search_dirs()

      for _, d in ipairs(dirs) do
	 local f = d .. "/" .. ver .. "/" .. block .. ".lua"
	 if utils.file_exists(f) then
	    fn = f
	    break
	 end
      end

      if not fn then
	 error("no module " .. block .. " found as file or under " ..
	       table.concat(dirs, ', ') .. " with version " .. ver)
      end
   end

   local cfg = { lua_file=fn }

   if active then
      local period = 100
      if type(active) == 'table' and active.period then
	 period = active.period
      end
      cfg.thread = 1
      cfg.period = period
   end

   b = ubx.block_create(nd, "ubx/luablock", name, cfg)

   if tgtstate then
      ubx.block_tostate(b, tgtstate)
   end

   return b
end

return M
