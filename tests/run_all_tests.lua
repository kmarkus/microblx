#!/usr/bin/luajit
--
-- Run all test suites in a single luaunit invocation.
-- Each test file uses table-based suites (TestXxx) and guards its
-- os.exit() behind `if not _RUNNER`, so dofile just registers the
-- test classes in _G without exiting.
--

_RUNNER = true

local p = io.popen('ls tests/test_*.lua')
for f in p:lines() do
   dofile(f)
end
p:close()

os.exit(require("luaunit").LuaUnit.run())
