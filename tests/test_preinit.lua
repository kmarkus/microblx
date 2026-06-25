#!/usr/bin/luajit
--
-- Test the preinit/preexit lifecycle hooks:
--   - preinit extends the block interface (ports/configs) from static
--     config, while the block stays in 'preinit'
--   - ubx_block_init runs preinit automatically (idempotent)
--   - blocks without a preinit hook still work (backwards compat)
--   - do_configure applies values to preinit-created configs before init
--   - preexit runs on block removal
--

local lu = require("luaunit")
local ubx = require("ubx")
local ffi = require("ffi")

local assert_equals = lu.assert_equals
local assert_not_nil = lu.assert_not_nil
local assert_nil = lu.assert_nil
local assert_true = lu.assert_true

local nd

TestPreinit = {}

function TestPreinit.setupClass()
   nd = ubx.node_create("test_preinit")
   ubx.load_module(nd, "stdtypes")
   ubx.load_module(nd, "luablock")
end

function TestPreinit.teardownClass()
   if nd then ubx.node_rm(nd) end
   nd = nil
end

function TestPreinit:teardown()
   ubx.node_clear(nd)
end

-- a luablock whose preinit hook adds an inport "dynin"
local preinit_str = [[
   ubx = require("ubx")
   function preinit(b)
      return ubx.inport_add(b, "dynin", "added in preinit", 0, "int32_t", 1) == 0
   end
]]

-- preinit runs on an explicit ubx.block_preinit, extends the interface,
-- and leaves the block in 'preinit'
function TestPreinit:test_preinit_extends_interface()
   local b = ubx.block_create(nd, "ubx/luablock", "lb", { lua_str = preinit_str })
   assert_not_nil(b)

   -- not present right after create (port_get raises on missing port)
   assert_true(not pcall(ubx.port_get, b, "dynin"))

   assert_equals(ubx.block_preinit(b), 0)
   assert_equals(b:get_block_state(), 'preinit')   -- still preinit
   assert_not_nil(ubx.port_get(b, "dynin"))        -- interface extended

   assert_equals(ubx.block_init(b), 0)
   assert_equals(b:get_block_state(), 'inactive')
end

-- ubx.block_init runs preinit automatically when not called explicitly
function TestPreinit:test_init_runs_preinit_automatically()
   local b = ubx.block_create(nd, "ubx/luablock", "lb", { lua_str = preinit_str })
   assert_not_nil(b)

   assert_equals(ubx.block_init(b), 0)             -- no explicit preinit
   assert_equals(b:get_block_state(), 'inactive')
   assert_not_nil(ubx.port_get(b, "dynin"))        -- preinit ran anyway
end

-- a block without a preinit hook still initializes (backwards compat)
function TestPreinit:test_no_preinit_hook()
   local b = ubx.block_create(nd, "ubx/luablock", "lb",
			      { lua_str = "function init(b) return true end" })
   assert_not_nil(b)
   assert_equals(ubx.block_init(b), 0)
   assert_equals(b:get_block_state(), 'inactive')
end

-- do_configure applies a value to a config created by the preinit hook
-- (the config does not exist at create time, only after preinit)
function TestPreinit:test_do_configure_preinit_config()
   local lua_str = [[
      ubx = require("ubx")
      function preinit(b)
	 return ubx.config_add(b, "gain", "created in preinit", "int32_t") == 0
      end
   ]]
   local b = ubx.block_create(nd, "ubx/luablock", "lb", { lua_str = lua_str })
   assert_not_nil(b)
   assert_nil(ubx.config_get(b, "gain"))           -- not there yet

   ubx.do_configure(b, { gain = 42 })

   assert_equals(b:get_block_state(), 'inactive')
   local c = ubx.config_get(b, "gain")
   assert_not_nil(c)
   assert_equals(c:tolua(), 42)                     -- deferred value applied
end

-- preexit runs on block removal (observed via a marker file written by
-- the preexit hook)
function TestPreinit:test_preexit_runs_on_rm()
   local marker = os.tmpname()
   os.remove(marker)

   local lua_str = string.format([[
      function preexit(b) local f = io.open(%q, "w"); f:write("x"); f:close() end
   ]], marker)

   local b = ubx.block_create(nd, "ubx/luablock", "lb", { lua_str = lua_str })
   assert_not_nil(b)
   assert_equals(ubx.block_init(b), 0)
   assert_equals(ubx.block_cleanup(b), 0)           -- back to preinit
   assert_equals(ubx.block_rm(nd, "lb"), 0)         -- triggers preexit

   local f = io.open(marker, "r")
   assert_not_nil(f, "preexit hook did not run")
   f:close()
   os.remove(marker)
end

if not _RUNNER then os.exit( lu.LuaUnit.run() ) end
