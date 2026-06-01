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

local lu = require("luaunit")
-- Use JUnit XML output if JUNIT_OUTPUT env var is set, otherwise use default text output
if os.getenv("JUNIT_OUTPUT") then
   os.exit(lu.LuaUnit.run('--output', 'junit', '--name', os.getenv("JUNIT_OUTPUT")))
else
   os.exit(lu.LuaUnit.run())
end
