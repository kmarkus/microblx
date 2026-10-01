--
-- Tests for model parameters: bd.param, the parameter context and
-- the ubx-launch -D/--params options.
--

local luaunit = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")

local assert_equals = luaunit.assert_equals
local assert_nil = luaunit.assert_nil
local assert_true = luaunit.assert_true
local assert_str_contains = luaunit.assert_str_contains
local assert_not_str_contains = luaunit.assert_not_str_contains
local assert_error_msg_contains = luaunit.assert_error_msg_contains

local fmt = string.format

TestParams = {}

local _tmpdir
local _tmpfiles

--- write a file into the per-test tmp dir and return its path
local function wfile(name, content)
   local fn = _tmpdir .. "/" .. name
   local f = assert(io.open(fn, "w"))
   f:write(content)
   f:close()
   _tmpfiles[#_tmpfiles+1] = fn
   return fn
end

-- run the tools from the source tree, independent of PATH
local _src = debug.getinfo(1, 'S').source:match('@(.*)')
local SRC_DIR = (_src:match('(.+)/[^/]+$') or '.') .. "/.."
if SRC_DIR:sub(1,1) ~= '/' then SRC_DIR = io.popen("pwd"):read("*l") .. "/" .. SRC_DIR end
local UBX_LAUNCH = "luajit " .. SRC_DIR .. "/tools/ubx-launch"

--- run a shell command, return combined stdout+stderr and exit code.
--- a leading ubx-launch runs the source tree version.
local function run(cmd)
   cmd = cmd:gsub("^ubx%-launch", UBX_LAUNCH)
   local p = io.popen(cmd .. " 2>&1; echo \"rc=$?\"")
   local out = p:read("*a")
   p:close()
   local rc = tonumber(out:match("rc=(%d+)\n?$"))
   return out:gsub("rc=%d+\n?$", ""), rc
end

local function usc_tostr(s) return fmt("%q", s) end

-- strip trailing whitespace of each line (write_table pads all columns)
local function rtrim_lines(s) return (s:gsub("[ ]+\n", "\n")) end

--- call func(fd, ...) with a string buffer fd and return what it wrote
local function write_str(func, ...)
   local buf = {}
   func({ write = function(_, ...) for _,x in ipairs{...} do buf[#buf+1] = x end end }, ...)
   return table.concat(buf)
end

-- minimal valid model declaring the parameters given as usc code
local function model(decls, extra)
   return fmt([[
%s
return bd.system { %s }
]], decls or "", extra or "")
end

-- threshold model whose threshold is the parameter T
local THRES_USC = [[
local T = bd.param("T", 1.0, "threshold")
return bd.system {
   imports = { "stdtypes", "lfrb", "threshold" },
   blocks = { { name = "t1", type = "ubx/threshold" } },
   configurations = { { name = "t1", config = { threshold = T } } },
}
]]

function TestParams:setUp()
   local p = io.popen("mktemp -d")
   _tmpdir = p:read("*l")
   p:close()
   _tmpfiles = {}
end

function TestParams:tearDown()
   -- a failing test must not leave a context open for the next one
   pcall(bd.params_end)
   for _,f in ipairs(_tmpfiles) do os.remove(f) end
   os.remove(_tmpdir)
end

---
--- bd.param outside a context
---

function TestParams:test_param_no_context_returns_default()
   assert_equals(bd.param("A", 3, "help"), 3)
   assert_equals(bd.param("S", "x"), "x")
end

function TestParams:test_param_no_context_records_nothing()
   bd.load_str(model('local a = bd.param("A", 1)'), 'lua')
   bd.params_begin()
   local decls = bd.params_end()
   assert_equals(#decls, 0)
end

function TestParams:test_param_invalid_name()
   for _,n in ipairs{ "1A", "A-B", "A.B", "", "A B" } do
      assert_error_msg_contains("invalid parameter name",
				function() bd.param(n, 1) end)
   end
   assert_error_msg_contains("invalid parameter name",
			     function() bd.param(nil, 1) end)
   assert_error_msg_contains("invalid parameter name",
			     function() bd.param(1, 1) end)
   -- valid names
   assert_equals(bd.param("_a1", 1), 1)
   assert_equals(bd.param("abc_DEF_9", 1), 1)
end

function TestParams:test_param_invalid_default()
   assert_error_msg_contains("parameter A: default must be a number, string or function, got boolean",
			     function() bd.param("A", true) end)
   assert_error_msg_contains("got nil", function() bd.param("A") end)
   assert_error_msg_contains("got table", function() bd.param("A", {}) end)
end

function TestParams:test_param_invalid_help()
   assert_error_msg_contains("parameter A: help must be a string, got number",
			     function() bd.param("A", 1, 2) end)
end

---
--- values
---

local function eval(values, decls)
   local res
   bd.with_params(values, function()
      res = bd.load_str(model(decls), 'lua')
   end)
   return res
end

--- load a model declaring one parameter and return its value
local function param_val(values, default)
   local v
   bd.with_params(values, function()
      bd.load_str(model(), 'lua')
      v = bd.param("P", default)
   end)
   return v
end

function TestParams:test_value_default_when_not_given()
   assert_equals(param_val({}, 5), 5)
   assert_equals(param_val({}, "s"), "s")
end

function TestParams:test_value_number_conversion()
   assert_equals(param_val({P="500"}, 1000), 500)
   assert_equals(param_val({P="-2.5"}, 0), -2.5)
   assert_equals(param_val({P="1e3"}, 0), 1000)
   assert_equals(param_val({P="0x10"}, 0), 16)
   assert_equals(type(param_val({P="7"}, 0)), "number")
end

function TestParams:test_value_invalid_number()
   assert_error_msg_contains('parameter P: invalid number "1ms"',
			     function() param_val({P="1ms"}, 1000) end)
   assert_error_msg_contains('parameter P: invalid number ""',
			     function() param_val({P=""}, 1000) end)
end

function TestParams:test_value_number_given()
   -- number values are taken as is or converted to string
   assert_equals(param_val({P=7}, 1000), 7)
   assert_equals(param_val({P=-2.5}, 0), -2.5)
   assert_equals(param_val({P=7}, "x"), "7")
   assert_equals(type(param_val({P=7}, "x")), "string")
end

function TestParams:test_value_string()
   assert_equals(param_val({P="abc"}, "x"), "abc")
   assert_equals(param_val({P="500"}, "x"), "500")
   assert_equals(param_val({P=""}, "x"), "")
   assert_equals(param_val({P="a=b c"}, "x"), "a=b c")
end

function TestParams:test_value_applied_to_launched_block()
   local m = bd.with_params({T="7.5"}, function()
      return bd.load_str(THRES_USC, 'lua')
   end)
   local nd = m:launch{ nodename="test_params_launch", nostart=true }
   local b = ubx.block_get(nd, "t1")
   assert_equals(ubx.data_tolua(ubx.config_get(b, "threshold").value), 7.5)
   ubx.node_rm(nd)
end

---
--- context handling
---

function TestParams:test_begin_invalid_values()
   -- no source location prefix, also when called through with_params
   local ok, e = pcall(bd.with_params, {["A-B"]="1"}, function() end)
   assert_equals(e, 'invalid parameter name "A-B"')
   ok, e = pcall(bd.with_params, {A=true}, function() end)
   assert_equals(e, "parameter A: value must be a string or number, got boolean")
   assert_error_msg_contains("got table", function() bd.params_begin({A={}}) end)
   -- a failed begin leaves no context open
   assert_error_msg_contains("no parameter context open", bd.params_end)
end

function TestParams:test_nested_begin_fails()
   bd.params_begin()
   assert_error_msg_contains("already open", bd.params_begin)
   bd.params_end()
end

function TestParams:test_end_without_begin_fails()
   assert_error_msg_contains("no parameter context open", bd.params_end)
end

function TestParams:test_unknown_parameter()
   assert_error_msg_contains("unknown parameter PERIDO; declared: PERIOD GRIPPER",
      function()
	 eval({PERIDO="1"}, 'bd.param("PERIOD", 1); bd.param("GRIPPER", 0)')
      end)
end

function TestParams:test_unknown_parameters_sorted()
   assert_error_msg_contains("unknown parameters X Y Z; declared: A",
      function() eval({Z="1", X="1", Y="1"}, 'bd.param("A", 1)') end)
end

function TestParams:test_unknown_parameter_none_declared()
   assert_error_msg_contains("unknown parameter A; declared: none",
			     function() eval({A="1"}) end)
end

function TestParams:test_unknown_closes_context()
   pcall(eval, {A="1"})
   assert_error_msg_contains("no parameter context open", bd.params_end)
end

function TestParams:test_with_params_closes_context_on_error()
   assert_error_msg_contains("boom", function()
      bd.with_params({A="1"}, function() error("boom") end)
   end)
   assert_error_msg_contains("no parameter context open", bd.params_end)
   -- the load error is reported, not the unknown parameter
   assert_error_msg_contains("failed to parse lua usc", function()
      bd.with_params({A="1"}, function() return bd.load_str("syntax error(", 'lua') end)
   end)
end

function TestParams:test_with_params_results()
   local res, decls, warnings = bd.with_params({}, function()
      return bd.load_str(model('bd.param("B", 2, "b help"); bd.param("A", "x")'), 'lua')
   end)
   assert_true(bd.is_system(res))
   assert_equals(#warnings, 0)
   assert_equals(#decls, 2)
   -- declaration order
   assert_equals(decls[1], { name="B", default=2, help="b help", file="<string>" })
   assert_equals(decls[2].name, "A")
   assert_nil(decls[2].help)
end

---
--- files and composition
---

function TestParams:test_load_records_file()
   local fn = wfile("a.usc", model('bd.param("A", 1)'))
   local _, decls = bd.with_params({}, function() return bd.load(fn) end)
   assert_equals(decls[1].file, fn)
end

function TestParams:test_load_error_names_file()
   local fn = wfile("a.usc", model('bd.param("A", 1)'))
   assert_error_msg_contains("failed to load " .. fn .. "\nparameter A: invalid number",
      function() bd.with_params({A="x"}, function() return bd.load(fn) end) end)
end

function TestParams:test_nested_load_inherits_context()
   local sub = wfile("sub.usc", model('local R = bd.param("RMAX", 256, "max")',
				      'configurations = { { name="x", config={ v=R } } }'))
   local top = wfile("top.usc", model('local P = bd.param("PERIOD", 1000)',
				      fmt('subsystems = { s = bd.load(%s) }', usc_tostr(sub))))
   local m, decls = bd.with_params({RMAX="100"}, function() return bd.load(top) end)
   assert_equals(m.subsystems.s.configurations[1].config.v, 100)
   assert_equals(#decls, 2)
   assert_equals(decls[1].name, "PERIOD")
   assert_equals(decls[1].file, top)
   assert_equals(decls[2].name, "RMAX")
   assert_equals(decls[2].file, sub)
end

function TestParams:test_nested_load_via_require()
   -- a model that requires blockdiagram itself shares the context
   local sub = wfile("sub.usc", [[
local bd = require("blockdiagram")
return bd.system { configurations = { { name="x", config={ v=bd.param("S", 1) } } } }
]])
   local top = wfile("top.usc", model(nil, fmt('subsystems = { s = bd.load(%s) }', usc_tostr(sub))))
   local m = bd.with_params({S="2"}, function() return bd.load(top) end)
   assert_equals(m.subsystems.s.configurations[1].config.v, 2)
end

function TestParams:test_file_restored_after_nested_load()
   local sub = wfile("sub.usc", model('bd.param("S", 1)'))
   local top = wfile("top.usc", fmt([[
local sub = bd.load(%s)
bd.param("T", 1)
return bd.system { subsystems = { s = sub } }
]], usc_tostr(sub)))
   local _, decls = bd.with_params({}, function() return bd.load(top) end)
   assert_equals(decls[1].file, sub)
   assert_equals(decls[2].file, top)
end

function TestParams:test_file_restored_after_failed_nested_load()
   local bad = wfile("bad.usc", "error('bad model')")
   local top = wfile("top.usc", fmt([[
pcall(bd.load, %s)
bd.param("T", 1)
return bd.system {}
]], usc_tostr(bad)))
   local _, decls = bd.with_params({}, function() return bd.load(top) end)
   assert_equals(decls[1].file, top)
end

function TestParams:test_nested_load_error_names_files()
   local sub = wfile("sub.usc", model('bd.param("S", 1)'))
   local top = wfile("top.usc", model(nil, fmt('subsystems = { s = bd.load(%s) }', usc_tostr(sub))))
   local ok, e = pcall(bd.with_params, {S="x"}, function() return bd.load(top) end)
   assert_true(not ok)
   assert_str_contains(e, "failed to load " .. top)
   assert_str_contains(e, "failed to load " .. sub)
   assert_str_contains(e, 'parameter S: invalid number "x"')
end

function TestParams:test_submodel_included_twice()
   local sub = wfile("sub.usc", model('local R = bd.param("R", 1)',
				      'configurations = { { name="x", config={ v=R } } }'))
   local top = wfile("top.usc", model(nil, fmt('subsystems = { a = bd.load(%s), b = bd.load(%s) }',
					       usc_tostr(sub), usc_tostr(sub))))
   local m, decls = bd.with_params({R="5"}, function() return bd.load(top) end)
   assert_equals(#decls, 1)
   assert_equals(m.subsystems.a.configurations[1].config.v, 5)
   assert_equals(m.subsystems.b.configurations[1].config.v, 5)
end

function TestParams:test_merged_files_share_context()
   local a = wfile("a.usc", model('bd.param("A", 1)'))
   local b = wfile("b.usc", model('local v = bd.param("B", "x")',
				  'configurations = { { name="x", config={ v=v } } }'))
   local m, decls = bd.with_params({B="y"}, function()
      local m = bd.load(a)
      m:merge(bd.load(b), true)
      return m
   end)
   assert_equals(#decls, 2)
   assert_equals(m.configurations[1].config.v, "y")
end

function TestParams:test_json_declares_nothing()
   local fn = wfile("a.json", '{ "blocks": [] }')
   local ok, json = pcall(bd.load, fn)
   if not ok then luaunit.skip("no json library") end
   local _, decls = bd.with_params({}, function() return bd.load(fn) end)
   assert_equals(#decls, 0)
   assert_error_msg_contains("unknown parameter A; declared: none", function()
      bd.with_params({A="1"}, function() return bd.load(fn) end)
   end)
end

function TestParams:test_conditional_declaration_not_taken()
   -- documented limitation: a declaration in a branch not taken is unknown
   assert_error_msg_contains("unknown parameter B", function()
      eval({B="1"}, 'if false then bd.param("B", 1) end')
   end)
end

---
--- repeated declarations
---

function TestParams:test_repeated_same_default()
   local _, decls, warnings = bd.with_params({}, function()
      bd.load_str(model('bd.param("A", 1, "first"); bd.param("A", 1, "second")'), 'lua')
   end)
   assert_equals(#decls, 1)
   assert_equals(decls[1].help, "first")
   assert_equals(#warnings, 0)
end

function TestParams:test_conflicting_default_is_error()
   local a = wfile("a.usc", model('bd.param("PERIOD", 1000)'))
   local b = wfile("b.usc", model('bd.param("PERIOD", 500)'))
   local ok, e = pcall(bd.with_params, {}, function()
      bd.load(a); bd.load(b)
   end)
   assert_true(not ok)
   assert_str_contains(e, fmt("parameter PERIOD: default 500 in %s conflicts with 1000 in %s", b, a))
end

function TestParams:test_conflicting_type_is_error()
   assert_error_msg_contains('parameter A: default "1" in <string> conflicts with 1 in <string>',
			     function() eval({}, 'bd.param("A", 1); bd.param("A", "1")') end)
end

function TestParams:test_conflicting_default_overridden_is_warning()
   local a = wfile("a.usc", model('local v = bd.param("PERIOD", 1000)',
				  'configurations = { { name="x", config={ v=v } } }'))
   local b = wfile("b.usc", model('local v = bd.param("PERIOD", 500)',
				  'configurations = { { name="y", config={ v=v } } }'))
   local m, decls, warnings = bd.with_params({PERIOD="200"}, function()
      local m = bd.load(a)
      m:merge(bd.load(b), true)
      return m
   end)
   assert_equals(#decls, 1)
   assert_equals(decls[1].default, 1000)
   assert_equals(warnings, {
      fmt("parameter PERIOD: default 500 in %s conflicts with 1000 in %s", b, a) })
   -- both files read the overriding value
   for _,c in ipairs(m.configurations) do assert_equals(c.config.v, 200) end
end

function TestParams:test_repeated_nan_default()
   -- nan ~= nan, but two nan defaults do not conflict
   local _, decls, warnings = bd.with_params({}, function()
      bd.load_str(model('bd.param("P", 0/0); bd.param("P", 0/0)'), 'lua')
   end)
   assert_equals(#decls, 1)
   assert_equals(#warnings, 0)
   assert_error_msg_contains("parameter P: default 1 in <string> conflicts with nan",
			     function() eval({}, 'bd.param("P", 0/0); bd.param("P", 1)') end)
   assert_error_msg_contains("parameter P: default nan in <string> conflicts with 1",
			     function() eval({}, 'bd.param("P", 1); bd.param("P", 0/0)') end)
end

function TestParams:test_conflicting_type_overridden_is_warning()
   local _, _, warnings = bd.with_params({A="2"}, function()
      bd.load_str(model('bd.param("A", 1); bd.param("A", "x")'), 'lua')
   end)
   assert_equals(#warnings, 1)
end

---
--- checks
---

local function positive(v)
   return v > 0, "must be > 0"
end

-- callable table in the style of a tableshape type: true or nil, err
local positive_shape = setmetatable({}, {
   __call = function(_, v)
      if type(v) == 'number' and v > 0 then return true end
      return nil, "expected number > 0"
   end
})

--- declare P with default and check in a context with values, return the value
local function checked_val(values, default, check)
   local v
   bd.with_params(values, function() v = bd.param("P", default, "h", check) end)
   return v
end

function TestParams:test_check_passes()
   assert_equals(checked_val({}, 5, positive), 5)
   assert_equals(checked_val({P="7"}, 5, positive), 7)
   assert_equals(bd.param("P", 5, "h", positive), 5)
end

function TestParams:test_value_nan_inf()
   -- accepted as numbers, a check can reject them
   assert_equals(param_val({P="inf"}, 0), math.huge)
   assert_equals(param_val({P="-inf"}, 0), -math.huge)
   assert_equals(param_val({P="1e400"}, 0), math.huge)
   assert_equals(param_val({P=math.huge}, 0), math.huge)
   local v = param_val({P="nan"}, 0)
   assert_true(v ~= v)
   local function finite(x) return x == x and x > -math.huge and x < math.huge, "must be finite" end
   assert_error_msg_contains("parameter P: invalid value inf: must be finite",
			     function() checked_val({P="inf"}, 0, finite) end)
   assert_error_msg_contains("parameter P: invalid value nan: must be finite",
			     function() checked_val({P="nan"}, 0, finite) end)
end

function TestParams:test_check_value_fails()
   assert_error_msg_contains("parameter P: invalid value -5: must be > 0",
			     function() checked_val({P="-5"}, 5, positive) end)
end

function TestParams:test_check_value_fails_without_message()
   assert_error_msg_contains("parameter P: invalid value 0: check failed",
      function() checked_val({P="0"}, 5, function(v) return v > 0 end) end)
   assert_error_msg_contains("parameter P: invalid value 0: check failed",
      function() checked_val({P="0"}, 5, function(v) if v > 0 then return true end end) end)
end

function TestParams:test_check_truthy_result_passes()
   -- e.g. tableshape may return a state object on success
   assert_equals(checked_val({P="3"}, 5, function() return {} end), 3)
end

function TestParams:test_check_default_fails()
   -- outside a context, inside one, and even if a valid value is given
   assert_error_msg_contains("parameter P: invalid default 0: must be > 0",
			     function() bd.param("P", 0, "h", positive) end)
   assert_error_msg_contains("parameter P: invalid default 0: must be > 0",
			     function() checked_val({}, 0, positive) end)
   assert_error_msg_contains("parameter P: invalid default 0: must be > 0",
			     function() checked_val({P="3"}, 0, positive) end)
end

function TestParams:test_check_gets_converted_value()
   local seen = {}
   local function rec(v) seen[#seen+1] = v; return true end
   checked_val({P="500"}, 1000, rec)
   assert_equals(seen, { 1000, 500 })
   assert_equals(type(seen[2]), "number")

   seen = {}
   checked_val({P="500"}, "x", rec)
   assert_equals(seen, { "x", "500" })
   assert_equals(type(seen[2]), "string")
end

function TestParams:test_check_string_param()
   local function mode(v) return v == "fifo" or v == "rr", "must be fifo or rr" end
   assert_equals(checked_val({P="rr"}, "fifo", mode), "rr")
   assert_error_msg_contains('parameter P: invalid value "other": must be fifo or rr',
			     function() checked_val({P="other"}, "fifo", mode) end)
end

function TestParams:test_check_not_called_on_conversion_error()
   local calls = 0
   assert_error_msg_contains('parameter P: invalid number "1ms"', function()
      checked_val({P="1ms"}, 1, function() calls = calls + 1; return true end)
   end)
   -- only the default was checked
   assert_equals(calls, 1)
end

function TestParams:test_check_callable_table()
   assert_equals(checked_val({P="2"}, 1, positive_shape), 2)
   assert_error_msg_contains('parameter P: invalid value -3: expected number > 0',
			     function() checked_val({P="-3"}, 1, positive_shape) end)
   assert_error_msg_contains('parameter P: invalid default "y": expected number > 0',
			     function() checked_val({}, "y", positive_shape) end)
end

function TestParams:test_check_not_callable()
   assert_error_msg_contains("parameter P: check must be callable, got number",
			     function() bd.param("P", 1, "h", 2) end)
   assert_error_msg_contains("parameter P: check must be callable, got table",
			     function() bd.param("P", 1, "h", {}) end)
   assert_error_msg_contains("parameter P: check must be callable, got table",
			     function() bd.param("P", 1, "h", setmetatable({}, {})) end)
   assert_error_msg_contains("parameter P: check must be callable, got string",
			     function() bd.param("P", 1, "h", "x") end)
   -- nil is no check
   assert_equals(bd.param("P", 1, "h", nil), 1)
end

function TestParams:test_check_raises()
   assert_error_msg_contains("parameter P: check failed: ",
      function() checked_val({}, 1, function() error("boom") end) end)
   assert_error_msg_contains("boom",
      function() checked_val({}, 1, function() error("boom") end) end)
end

function TestParams:test_check_in_submodel_names_file()
   local sub = wfile("sub.usc", model('bd.param("R", 1, "h", function(v) return v < 10, "must be < 10" end)'))
   local top = wfile("top.usc", model(nil, fmt('subsystems = { s = bd.load(%s) }', usc_tostr(sub))))
   local ok, e = pcall(bd.with_params, {R="20"}, function() return bd.load(top) end)
   assert_true(not ok)
   assert_str_contains(e, "failed to load " .. sub)
   assert_str_contains(e, "parameter R: invalid value 20: must be < 10")
end

function TestParams:test_check_per_declaration()
   -- each declaration runs its own check, checks are not compared
   local a = wfile("a.usc", model('bd.param("P", 5, "h", function(v) return v < 100, "a: < 100" end)'))
   local b = wfile("b.usc", model('bd.param("P", 5, "h", function(v) return v < 10, "b: < 10" end)'))
   local function load2(values)
      return bd.with_params(values, function() bd.load(a); bd.load(b) end)
   end
   local _, decls, warnings = load2({P="8"})
   assert_equals(#decls, 1)
   assert_equals(#warnings, 0)
   assert_error_msg_contains("parameter P: invalid value 50: b: < 10",
			     function() load2({P="50"}) end)
end

function TestParams:test_launch_check_fails()
   local fn = wfile("a.usc", model('bd.param("P", 1, "h", function(v) return v > 0, "must be > 0" end)'))
   local out, rc = run("ubx-launch -c " .. fn .. " -D P=-1 --params")
   assert_equals(rc, 1)
   assert_str_contains(out, "parameter P: invalid value -1: must be > 0")
   out, rc = run("ubx-launch -c " .. fn .. " -D P=2 --params")
   assert_equals(rc, 0, out)
end

---
--- required parameters
---

function TestParams:test_required_value_converted()
   assert_equals(param_val({P="5"}, tonumber), 5)
   assert_equals(param_val({P=5}, tostring), "5")
   assert_equals(param_val({P="a,b"}, function(v) return { v:match("(%a),(%a)") } end), { "a", "b" })
end

function TestParams:test_required_missing()
   assert_error_msg_contains("parameter P: required, set with -D P=VALUE",
			     function() param_val({}, tonumber) end)
   assert_error_msg_contains("parameter P: required, but no parameter context open",
			     function() bd.param("P", tonumber) end)
end

function TestParams:test_required_invalid_value()
   assert_error_msg_contains('parameter P: invalid value "x"',
			     function() param_val({P="x"}, tonumber) end)
end

function TestParams:test_required_check()
   -- the check gets the converted value and is not run without one
   assert_equals(checked_val({P="2"}, tonumber, positive), 2)
   assert_error_msg_contains("parameter P: invalid value -1: must be > 0",
			     function() checked_val({P="-1"}, tonumber, positive) end)
end

function TestParams:test_required_conflict()
   local _, decls = bd.with_params({P="1"}, function()
      bd.load_str(model('bd.param("P", tonumber); bd.param("P", tonumber)'), 'lua')
   end)
   assert_equals(#decls, 1)
   assert_error_msg_contains("parameter P: default <required> in <string> conflicts with 1",
			     function() eval({}, 'bd.param("P", 1); bd.param("P", tonumber)') end)
   local _, _, warnings = bd.with_params({P="1"}, function()
      bd.load_str(model('bd.param("P", tonumber); bd.param("P", tostring)'), 'lua')
   end)
   assert_equals(warnings, { "parameter P: default <required> in <string> conflicts with <required> in <string>" })
end

function TestParams:test_required_list_mode()
   local res, decls, _, err = bd.with_params({}, function()
      return bd.load_str(model('bd.param("A", tonumber); bd.param("B", 2)'), 'lua')
   end, true)
   assert_true(bd.is_system(res))
   assert_nil(err)
   assert_equals(#decls, 2)
   -- a load failing on the nil returns the declarations so far
   res, decls, _, err = bd.with_params({}, function()
      return bd.load_str(model('local a = bd.param("A", tonumber) * 2; bd.param("B", 2)'), 'lua')
   end, true)
   assert_nil(res)
   assert_equals(#decls, 1)
   assert_str_contains(err, "arithmetic")
   assert_error_msg_contains("no parameter context open", bd.params_end)
   -- other errors are raised also in list mode
   assert_error_msg_contains("boom", function()
      bd.with_params({}, function() error("boom") end, true)
   end)
end

---
--- helpers
---

function TestParams:test_parse_defines()
   assert_equals(bd.parse_defines(nil), {})
   assert_equals(bd.parse_defines("A=1"), {A="1"})
   assert_equals(bd.parse_defines({"A=1", "B=x=y", "C="}), {A="1", B="x=y", C=""})
   -- last one wins
   assert_equals(bd.parse_defines({"A=1", "A=2"}), {A="2"})
end

function TestParams:test_parse_defines_invalid()
   assert_error_msg_contains("invalid define 'A', expected NAME=VALUE",
			     function() bd.parse_defines("A") end)
   assert_error_msg_contains("invalid define '=1': invalid parameter name ''",
			     function() bd.parse_defines("=1") end)
   assert_error_msg_contains("invalid parameter name 'A-B'",
			     function() bd.parse_defines("A-B=1") end)
   assert_error_msg_contains("expected NAME=VALUE",
			     function() bd.parse_defines("") end)
end

function TestParams:test_params_write()
   assert_equals(write_str(bd.params_write, {}), "no parameters declared\n")
   assert_equals(rtrim_lines(write_str(bd.params_write, {
		    { name="PERIOD", default=1000, help="period [us]" },
		    { name="N", default="x" },
		    { name="R", default=tonumber } })),
		 ' name    default     help\n' ..
		 ' PERIOD  1000        period [us]\n' ..
		 ' N       "x"\n' ..
		 ' R       <required>\n')
end

---
--- ubx-launch
---

function TestParams:test_launch_params()
   local fn = wfile("a.usc", model('bd.param("PERIOD", 1000, "period [us]"); bd.param("N", "x", "name")'))
   local out, rc = run("ubx-launch -c " .. fn .. " --params")
   assert_equals(rc, 0)
   assert_equals(rtrim_lines(out), ' name    default  help\n PERIOD  1000     period [us]\n N       "x"      name\n')
end

function TestParams:test_launch_params_required()
   local fn = wfile("a.usc", model('bd.param("R", tonumber, "r"); bd.param("N", 1)'))
   local out, rc = run("ubx-launch -c " .. fn .. " --params")
   assert_equals(rc, 0, out)
   assert_equals(rtrim_lines(out), ' name  default     help\n R     <required>  r\n N     1\n')
   out, rc = run("ubx-launch -c " .. fn .. " --nostart")
   assert_equals(rc, 1)
   assert_str_contains(out, "parameter R: required, set with -D R=VALUE")
   -- listing stops where the model fails on the missing value
   fn = wfile("b.usc", model('local r = bd.param("R", tonumber) + 1; bd.param("N", 1)'))
   out, rc = run("ubx-launch -c " .. fn .. " --params")
   assert_equals(rc, 1)
   assert_str_contains(out, " R     <required>")
   assert_not_str_contains(out, " N ")
   assert_str_contains(out, "error: listing incomplete: ")
end

function TestParams:test_launch_params_none()
   local fn = wfile("a.usc", model())
   local out, rc = run("ubx-launch -c " .. fn .. " --params")
   assert_equals(rc, 0)
   assert_equals(out, "no parameters declared\n")
end

function TestParams:test_launch_params_merged()
   local a = wfile("a.usc", model('bd.param("A", 1)'))
   local b = wfile("b.usc", model('bd.param("B", 2)'))
   local out, rc = run(fmt("ubx-launch -c %s,%s --params", a, b))
   assert_equals(rc, 0)
   assert_str_contains(out, " A     1")
   assert_str_contains(out, " B     2")
end

function TestParams:test_launch_unknown()
   local fn = wfile("a.usc", model('bd.param("PERIOD", 1000)'))
   local out, rc = run("ubx-launch -c " .. fn .. " -D PERIDO=1 --nostart")
   assert_equals(rc, 1)
   assert_equals(out, "error: unknown parameter PERIDO; declared: PERIOD\n")
end

function TestParams:test_launch_invalid_value()
   local fn = wfile("a.usc", model('bd.param("PERIOD", 1000)'))
   local out, rc = run("ubx-launch -c " .. fn .. " -D PERIOD=1ms --params")
   assert_equals(rc, 1)
   assert_str_contains(out, 'parameter PERIOD: invalid number "1ms"')
end

function TestParams:test_launch_invalid_define()
   local fn = wfile("a.usc", model('bd.param("A", 1)'))
   for _,d in ipairs{ "A", "=1", "1A=1" } do
      local out, rc = run(fmt("ubx-launch -c %s -D '%s' --params", fn, d))
      assert_equals(rc, 1)
      assert_str_contains(out, "error: invalid define")
   end
end

function TestParams:test_launch_define_forms()
   -- short, attached and long forms, value containing '='
   local fn = wfile("a.usc", model('bd.param("A", 1); bd.param("B", "x"); bd.param("C", "x")'))
   local out, rc = run(fmt("ubx-launch -c %s -D A=2 -DB=y --define=C=k=v --params", fn))
   assert_equals(rc, 0, out)
end

function TestParams:test_launch_empty_rejects_define()
   for _,o in ipairs{ "-D A=1", "--params" } do
      local out, rc = run("ubx-launch -e " .. o)
      assert_equals(rc, 1)
      assert_equals(out, "error: -D and --params require a model (-c)\n")
   end
end

function TestParams:test_launch_conflict()
   local a = wfile("a.usc", model('bd.param("P", 1)'))
   local b = wfile("b.usc", model('bd.param("P", 2)'))
   local conf = fmt("ubx-launch -c %s,%s --params", a, b)

   local out, rc = run(conf)
   assert_equals(rc, 1)
   assert_str_contains(out, fmt("error: failed to load %s\nparameter P: default 2 in %s conflicts with 1 in %s", b, b, a))

   out, rc = run(conf .. " -D P=3")
   assert_equals(rc, 0)
   assert_str_contains(out, fmt("warning: parameter P: default 2 in %s conflicts with 1 in %s", b, a))

   out, rc = run(conf .. " -D P=3 --werror")
   assert_equals(rc, 1)
   assert_str_contains(out, "error: treating warnings as errors (--werror)")
end

function TestParams:test_launch_applies_value()
   -- --nostart --validate would not launch; launch for 1s instead and
   -- check the value via a model that fails on a wrong value
   local fn = wfile("a.usc", [[
local T = bd.param("T", 1.0)
assert(T == 7.5, "T is "..tostring(T))
return bd.system {
   imports = { "stdtypes", "lfrb", "threshold" },
   blocks = { { name = "t1", type = "ubx/threshold" } },
   configurations = { { name = "t1", config = { threshold = T } } },
}
]])
   local out, rc = run("ubx-launch -c " .. fn .. " -D T=7.5 --nostart -t 0")
   assert_equals(rc, 0, out)
   out, rc = run("ubx-launch -c " .. fn .. " --nostart -t 0")
   assert_equals(rc, 1)
   assert_str_contains(out, "T is 1")
end

function TestParams:test_launch_without_params_unchanged()
   -- models without parameters and without -D behave as before
   local fn = wfile("a.usc", model())
   local out, rc = run("ubx-launch -c " .. fn .. " --nostart -t 0")
   assert_equals(rc, 0, out)
   assert_not_str_contains(out, "warning")
end

---
--- examples using parameters
---

local EXAMPLES = SRC_DIR .. "/examples/usc/"

local function load_example(name, values)
   return bd.with_params(values or {}, function() return bd.load(EXAMPLES .. name) end)
end

local function find_cfg(m, name)
   for _,c in ipairs(m.configurations) do
      if c.name == name then return c.config end
   end
end

function TestParams:test_example_threshold()
   local m, decls = load_example("threshold.usc")
   assert_equals(#decls, 3)
   local c = find_cfg(m, "trigger")
   assert_equals(c.period.usec, 1000)
   assert_equals(c.sched_policy, "SCHED_OTHER")
   assert_equals(c.sched_priority, 0)

   m = load_example("threshold.usc", { PERIOD="500", SCHED_POLICY="SCHED_FIFO", SCHED_PRIORITY="50" })
   c = find_cfg(m, "trigger")
   assert_equals(c.period.usec, 500)
   assert_equals(c.sched_policy, "SCHED_FIFO")
   assert_equals(c.sched_priority, 50)

   assert_error_msg_contains('parameter SCHED_POLICY: invalid value "SCHED_DEADLINE": must be one of',
      function() load_example("threshold.usc", { SCHED_POLICY="SCHED_DEADLINE" }) end)
   assert_error_msg_contains("parameter SCHED_PRIORITY: invalid value 100",
      function() load_example("threshold.usc", { SCHED_PRIORITY="100" }) end)
   assert_error_msg_contains("parameter PERIOD: invalid value 0",
      function() load_example("threshold.usc", { PERIOD="0" }) end)
   assert_error_msg_contains("parameter PERIOD: invalid value 1.5",
      function() load_example("threshold.usc", { PERIOD="1.5" }) end)
end

function TestParams:test_example_threshold_launch()
   local out, rc = run(fmt("ubx-launch -c %sthreshold.usc -D PERIOD=500 -t 1", EXAMPLES))
   assert_equals(rc, 0, out)
end

function TestParams:test_example_large_number_blocks()
   local m = load_example("large-number-blocks.usc")
   assert_equals(#m.blocks, 1 + 4 * 50)
   m = load_example("large-number-blocks.usc", { N="3" })
   assert_equals(#m.blocks, 1 + 4 * 3)
   assert_error_msg_contains("parameter N: invalid value 0: must be an integer >= 1",
      function() load_example("large-number-blocks.usc", { N="0" }) end)
   assert_error_msg_contains("parameter N: invalid value 2.5",
      function() load_example("large-number-blocks.usc", { N="2.5" }) end)
end

function TestParams:test_example_netsink()
   local m, decls = load_example("netsink.usc")
   assert_equals(#decls, 4)
   local lua_str = find_cfg(m, "sink").lua_str
   assert_str_contains(lua_str, 'transport = "udp"')
   assert_str_contains(lua_str, 'format    = "json"')
   assert_str_contains(lua_str, 'host = "127.0.0.1"')
   assert_str_contains(lua_str, 'port = 9870')

   m = load_example("netsink.usc", { TRANSPORT="zmq", FORMAT="msgpack", PORT="1234" })
   lua_str = find_cfg(m, "sink").lua_str
   assert_str_contains(lua_str, 'transport = "zmq"')
   assert_str_contains(lua_str, 'format    = "msgpack"')
   assert_str_contains(lua_str, 'uri  = "tcp://*:1234"')

   m = load_example("netsink.usc", { HOST="10.0.0.1" })
   assert_str_contains(find_cfg(m, "sink").lua_str, 'host = "10.0.0.1"')

   assert_error_msg_contains('parameter TRANSPORT: invalid value "tcp": must be one of udp, zmq',
      function() load_example("netsink.usc", { TRANSPORT="tcp" }) end)
   assert_error_msg_contains('parameter FORMAT: invalid value "xml"',
      function() load_example("netsink.usc", { FORMAT="xml" }) end)
   assert_error_msg_contains("parameter PORT: invalid value 70000",
      function() load_example("netsink.usc", { PORT="70000" }) end)
end

function TestParams:test_example_pid_ptrig_mixins()
   for _,e in ipairs{ "ptrig_nrt.usc", "ptrig_rt.usc", "ptrig_deadline.usc" } do
      local m = load_example("pid/" .. e)
      assert_equals(find_cfg(m, "ptrig_1").period.usec, 1000)
      m = load_example("pid/" .. e, { PERIOD="500" })
      assert_equals(find_cfg(m, "ptrig_1").period.usec, 500)
      assert_error_msg_contains("parameter PERIOD: invalid value 0",
	 function() load_example("pid/" .. e, { PERIOD="0" }) end)
   end
end

function TestParams:test_example_pid_merged_params()
   -- pid_test.usc loads pid.usc relative to the cwd
   local out, rc = run(fmt("cd %spid && %s -c pid_test.usc,ptrig_nrt.usc --params",
			   EXAMPLES, UBX_LAUNCH))
   assert_equals(rc, 0, out)
   assert_str_contains(out, " PERIOD  1000     ptrig period [us]")
end

function TestParams:test_example_params_listing()
   for _,e in ipairs{ "threshold.usc", "large-number-blocks.usc", "netsink.usc" } do
      local out, rc = run(fmt("ubx-launch -c %s%s --params", EXAMPLES, e))
      assert_equals(rc, 0, out)
      assert_not_str_contains(out, "no parameters declared")
   end
end

if not _RUNNER then
   os.exit(luaunit.LuaUnit.run())
end
