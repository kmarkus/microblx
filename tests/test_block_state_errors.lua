--
-- Test block state transitions: error paths and edge cases
--

local lu = require("luaunit")
local ffi = require("ffi")
local ubx = require("ubx")

local assert_equals = lu.assert_equals
local assert_not_nil = lu.assert_not_nil
local assert_not_equals = lu.assert_not_equals

local nd

TestBlockStateErrors = {}

function TestBlockStateErrors:setup()
   nd = ubx.node_create("test_block_state_errors", { loglevel = ffi.C.UBX_LOGLEVEL_WARN })
   ubx.load_module(nd, "stdtypes")
   ubx.load_module(nd, "random")
   ubx.load_module(nd, "luablock")
   ubx.load_module(nd, "lfrb")
end

function TestBlockStateErrors:teardown()
   if nd then ubx.node_rm(nd) end
   nd = nil
end

--- Test idempotent transitions: calling tostate with current state
function TestBlockStateErrors:TestIdempotentInactive()
   local b = ubx.block_create(nd, "ubx/random", "b_idem",
			       { min_max_config = { min = 1, max = 10 } })
   assert_not_nil(b)
   assert_equals(ubx.block_tostate(b, 'inactive'), 0)
   assert_equals(b.block_state, ffi.C.BLOCK_STATE_INACTIVE)
   -- calling again should be idempotent
   assert_equals(ubx.block_tostate(b, 'inactive'), 0)
   assert_equals(b.block_state, ffi.C.BLOCK_STATE_INACTIVE)
end

function TestBlockStateErrors:TestIdempotentActive()
   local b = ubx.block_create(nd, "ubx/random", "b_idem2",
			       { min_max_config = { min = 1, max = 10 } })
   assert_equals(ubx.block_tostate(b, 'active'), 0)
   assert_equals(b.block_state, ffi.C.BLOCK_STATE_ACTIVE)
   -- calling again should be idempotent (block_tostate returns 0 for same state)
   assert_equals(ubx.block_tostate(b, 'active'), 0)
   assert_equals(b.block_state, ffi.C.BLOCK_STATE_ACTIVE)
end

--- Test full lifecycle: preinit -> inactive -> active -> inactive -> preinit
function TestBlockStateErrors:TestFullLifecycle()
   local b = ubx.block_create(nd, "ubx/random", "b_lifecycle",
			       { min_max_config = { min = 1, max = 10 } })
   assert_equals(b.block_state, ffi.C.BLOCK_STATE_PREINIT)

   assert_equals(ubx.block_tostate(b, 'inactive'), 0)
   assert_equals(b.block_state, ffi.C.BLOCK_STATE_INACTIVE)

   assert_equals(ubx.block_tostate(b, 'active'), 0)
   assert_equals(b.block_state, ffi.C.BLOCK_STATE_ACTIVE)

   assert_equals(ubx.block_tostate(b, 'inactive'), 0)
   assert_equals(b.block_state, ffi.C.BLOCK_STATE_INACTIVE)

   assert_equals(ubx.block_tostate(b, 'preinit'), 0)
   assert_equals(b.block_state, ffi.C.BLOCK_STATE_PREINIT)
end

--- Test direct active->preinit transition (skip inactive)
function TestBlockStateErrors:TestActiveToPreinit()
   local b = ubx.block_create(nd, "ubx/random", "b_a2p",
			       { min_max_config = { min = 1, max = 10 } })
   assert_equals(ubx.block_tostate(b, 'active'), 0)
   assert_equals(b.block_state, ffi.C.BLOCK_STATE_ACTIVE)

   -- direct jump from active to preinit should work
   assert_equals(ubx.block_tostate(b, 'preinit'), 0)
   assert_equals(b.block_state, ffi.C.BLOCK_STATE_PREINIT)
end

--- Test that a luablock with a failing init returns error
function TestBlockStateErrors:TestFailingInit()
   local b = ubx.block_create(nd, "ubx/luablock", "b_fail_init",
			       { lua_str = "function init(b) error('intentional init failure') end" })
   assert_not_nil(b)
   local ret = ubx.block_init(b)
   assert_not_equals(ret, 0, "expected init to fail")
   assert_equals(b.block_state, ffi.C.BLOCK_STATE_PREINIT)
end

--- Test that a luablock with a failing start returns error
function TestBlockStateErrors:TestFailingStart()
   local b = ubx.block_create(nd, "ubx/luablock", "b_fail_start",
			       { lua_str = "function start(b) error('intentional start failure') end" })
   assert_not_nil(b)
   assert_equals(ubx.block_init(b), 0)
   assert_equals(b.block_state, ffi.C.BLOCK_STATE_INACTIVE)

   local ret = ubx.block_start(b)
   assert_not_equals(ret, 0, "expected start to fail")
   assert_equals(b.block_state, ffi.C.BLOCK_STATE_INACTIVE)
end

--- Test block_unload
function TestBlockStateErrors:TestBlockUnload()
   local b = ubx.block_create(nd, "ubx/random", "b_unload",
			       { min_max_config = { min = 1, max = 10 } })
   assert_equals(ubx.block_tostate(b, 'active'), 0)

   ubx.block_unload(nd, "b_unload")

   -- block should no longer exist
   local ok = pcall(ubx.block_get, nd, "b_unload")
   lu.assert_false(ok, "expected block_get to fail after unload")
end

--- Test num_blocks counting
function TestBlockStateErrors:TestNumBlocks()
   local cb0, ib0 = ubx.num_blocks(nd)

   ubx.block_create(nd, "ubx/random", "count_cb1",
		    { min_max_config = { min = 1, max = 10 } })
   ubx.block_create(nd, "ubx/random", "count_cb2",
		    { min_max_config = { min = 1, max = 10 } })

   local cb1, ib1 = ubx.num_blocks(nd)
   assert_equals(cb1, cb0 + 2)
   assert_equals(ib1, ib0)
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
