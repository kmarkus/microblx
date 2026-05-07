--
-- Test the threshold block
--

local lu = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")
local ffi = require("ffi")

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_WARN

local ni

TestThreshold = {}

function TestThreshold:teardown()
   if ni then ubx.node_rm(ni) end
   ni = nil
end

--- Test basic threshold crossing detection
function TestThreshold:TestBasicThreshold()
   local sys = bd.system {
      imports = { "stdtypes", "lfds_cyclic", "threshold" },
      blocks = {
	 { name = "thres1", type = "ubx/threshold" },
      },
      configurations = {
	 { name = "thres1", config = { threshold = 5.0 } },
      },
   }

   ni = sys:launch({ nodename = "TestThres", nostart = true, loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)

   local thres1 = ni:b("thres1")
   local p_in = ubx.port_clone_conn(thres1, "in", 1, nil, 7, 0)
   local p_state = ubx.port_clone_conn(thres1, "state", nil, 1, 7, 0)
   local p_event = ubx.port_clone_conn(thres1, "event", nil, 1, 7, 0)

   ubx.block_tostate(thres1, 'active')

   -- below threshold: state=0
   p_in:write(3.0)
   thres1:do_step()
   local _, sval = p_state:read()
   lu.assert_equals(sval:tolua(), 0)

   -- above threshold: state=1, should emit rising event
   p_in:write(7.0)
   thres1:do_step()
   _, sval = p_state:read()
   lu.assert_equals(sval:tolua(), 1)

   local elen, eval = p_event:read()
   lu.assert_true(tonumber(elen) > 0, "expected event on threshold crossing")
   -- access dir field directly via cdata to avoid tolua issue
   -- with ubx_timespec __eq metamethod
   local ev_cdata = eval:cdata()
   lu.assert_equals(tonumber(ev_cdata.dir), 1) -- rising

   -- still above: state=1, no new event
   p_in:write(8.0)
   thres1:do_step()
   _, sval = p_state:read()
   lu.assert_equals(sval:tolua(), 1)

   local elen2 = p_event:read()
   lu.assert_equals(tonumber(elen2), 0, "no event expected when state unchanged")

   -- back below: state=0, falling event
   p_in:write(2.0)
   thres1:do_step()
   _, sval = p_state:read()
   lu.assert_equals(sval:tolua(), 0)

   elen, eval = p_event:read()
   lu.assert_true(tonumber(elen) > 0, "expected event on falling crossing")
   ev_cdata = eval:cdata()
   lu.assert_equals(tonumber(ev_cdata.dir), 0) -- falling
end

--- Test threshold at boundary (exactly equal)
function TestThreshold:TestBoundaryValue()
   local sys = bd.system {
      imports = { "stdtypes", "lfds_cyclic", "threshold" },
      blocks = {
	 { name = "thres1", type = "ubx/threshold" },
      },
      configurations = {
	 { name = "thres1", config = { threshold = 5.0 } },
      },
   }

   ni = sys:launch({ nodename = "TestThresBound", nostart = true, loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)

   local thres1 = ni:b("thres1")
   local p_in = ubx.port_clone_conn(thres1, "in", 1, nil, 7, 0)
   local p_state = ubx.port_clone_conn(thres1, "state", nil, 1, 7, 0)

   ubx.block_tostate(thres1, 'active')

   -- exactly at threshold: should be state=0 (not strictly above)
   p_in:write(5.0)
   thres1:do_step()
   local _, sval = p_state:read()
   lu.assert_equals(sval:tolua(), 0)

   -- just above
   p_in:write(5.0000001)
   thres1:do_step()
   _, sval = p_state:read()
   lu.assert_equals(sval:tolua(), 1)
end

--- Test no data on input produces no output change
function TestThreshold:TestNoData()
   local sys = bd.system {
      imports = { "stdtypes", "lfds_cyclic", "threshold" },
      blocks = {
	 { name = "thres1", type = "ubx/threshold" },
      },
      configurations = {
	 { name = "thres1", config = { threshold = 5.0 } },
      },
   }

   ni = sys:launch({ nodename = "TestThresNoData", nostart = true, loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)

   local thres1 = ni:b("thres1")
   local p_state = ubx.port_clone_conn(thres1, "state", nil, 1, 7, 0)

   ubx.block_tostate(thres1, 'active')

   -- step without writing input
   thres1:do_step()
   local slen = p_state:read()
   -- no input means no output
   lu.assert_equals(tonumber(slen), 0)
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
