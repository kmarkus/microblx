--
-- Tests for the mux/demux blocks (ubx/mux, ubx/demux)
--

local lu = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")
local ffi = require("ffi")

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_WARN

local assert_equals = lu.assert_equals
local assert_not_nil = lu.assert_not_nil
local assert_true = lu.assert_true

local ni

TestMux = {}

function TestMux:teardown()
   if ni then ubx.node_rm(ni) end
   ni = nil
end

local function launch(btype, cfg, nodename)
   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "mux" },
      blocks = { { name = "b1", type = btype } },
      configurations = {
	 { name = "b1", config = cfg },
      },
   }

   ni = sys:launch({ nodename = nodename, nostart = true, loglevel = LOGLEVEL })
   assert_not_nil(ni)
   return ni:b("b1")
end

--- mux: three scalar inputs are combined into one vector output
function TestMux:TestMuxBasic()
   local b = launch("ubx/mux", { type = "double", nin = 3 }, "TestMuxBasic")

   local p_in = {}
   for i = 0, 2 do
      p_in[i] = ubx.port_clone_conn(b, "in" .. i, 1, nil, 7, 0)
   end
   local p_out = ubx.port_clone_conn(b, "out", nil, 1, 7, 0)
   ubx.block_tostate(b, 'active')

   p_in[0]:write(1.5)
   p_in[1]:write(-2.5)
   p_in[2]:write(3.5)
   b:do_step()

   local len, val = p_out:read()
   assert_equals(tonumber(len), 3)
   assert_equals(val:tolua(), { 1.5, -2.5, 3.5 })
end

--- mux: a stale input keeps its last value; all-stale steps emit nothing
function TestMux:TestMuxStaleInput()
   local b = launch("ubx/mux", { type = "double", nin = 2 }, "TestMuxStale")

   local p0 = ubx.port_clone_conn(b, "in0", 1, nil, 7, 0)
   local p1 = ubx.port_clone_conn(b, "in1", 1, nil, 7, 0)
   local p_out = ubx.port_clone_conn(b, "out", nil, 1, 7, 0)
   ubx.block_tostate(b, 'active')

   p0:write(1.0)
   p1:write(2.0)
   b:do_step()
   local _, val = p_out:read()
   assert_equals(val:tolua(), { 1.0, 2.0 })

   -- only in0 updates: in1 slot keeps its last value
   p0:write(10.0)
   b:do_step()
   _, val = p_out:read()
   assert_equals(val:tolua(), { 10.0, 2.0 })

   -- no input updates: no output
   b:do_step()
   local len = p_out:read()
   assert_equals(tonumber(len), 0)
end

--- demux: a vector input is split onto the scalar output ports
function TestMux:TestDemuxBasic()
   local b = launch("ubx/demux", { type = "int32_t", nout = 3 }, "TestDemuxBasic")

   local p_in = ubx.port_clone_conn(b, "in", 1, nil, 7, 0)
   local p_out = {}
   for i = 0, 2 do
      p_out[i] = ubx.port_clone_conn(b, "out" .. i, nil, 1, 7, 0)
   end
   ubx.block_tostate(b, 'active')

   p_in:write({ 11, -22, 33 })
   b:do_step()

   local exp = { [0] = 11, [1] = -22, [2] = 33 }
   for i = 0, 2 do
      local len, val = p_out[i]:read()
      assert_equals(tonumber(len), 1)
      assert_equals(val:tolua(), exp[i])
   end
end

--- demux: NODATA produces no outputs
function TestMux:TestDemuxNoData()
   local b = launch("ubx/demux", { type = "double", nout = 2 }, "TestDemuxNoData")

   local p_out = ubx.port_clone_conn(b, "out0", nil, 1, 7, 0)
   ubx.block_tostate(b, 'active')

   b:do_step()
   local len = p_out:read()
   assert_equals(tonumber(len), 0)
end

--- mux -> demux roundtrip through a connection
function TestMux:TestMuxDemuxRoundtrip()
   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "mux" },
      blocks = {
	 { name = "mux1", type = "ubx/mux" },
	 { name = "demux1", type = "ubx/demux" },
      },
      configurations = {
	 { name = "mux1", config = { type = "double", nin = 2 } },
	 { name = "demux1", config = { type = "double", nout = 2 } },
      },
      connections = {
	 { src = "mux1.out", tgt = "demux1.in" },
      },
   }

   ni = sys:launch({ nodename = "TestMuxRT", nostart = true, loglevel = LOGLEVEL })
   assert_not_nil(ni)

   local mux1, demux1 = ni:b("mux1"), ni:b("demux1")
   local p0 = ubx.port_clone_conn(mux1, "in0", 1, nil, 7, 0)
   local p1 = ubx.port_clone_conn(mux1, "in1", 1, nil, 7, 0)
   local q0 = ubx.port_clone_conn(demux1, "out0", nil, 1, 7, 0)
   local q1 = ubx.port_clone_conn(demux1, "out1", nil, 1, 7, 0)
   ubx.block_tostate(mux1, 'active')
   ubx.block_tostate(demux1, 'active')

   p0:write(4.5)
   p1:write(-4.5)
   mux1:do_step()
   demux1:do_step()

   local _, v0 = q0:read()
   local _, v1 = q1:read()
   assert_equals(v0:tolua(), 4.5)
   assert_equals(v1:tolua(), -4.5)
end

--- mux with in_len: sub-vectors are concatenated in port order
function TestMux:TestMuxSubvectors()
   local b = launch("ubx/mux", { type = "double", nin = 2, in_len = { 2, 3 } },
		    "TestMuxSubvec")

   local p0 = ubx.port_clone_conn(b, "in0", 1, nil, 7, 0)
   local p1 = ubx.port_clone_conn(b, "in1", 1, nil, 7, 0)
   local p_out = ubx.port_clone_conn(b, "out", nil, 1, 7, 0)
   ubx.block_tostate(b, 'active')

   p0:write({ 1, 2 })
   p1:write({ 3, 4, 5 })
   b:do_step()

   local len, val = p_out:read()
   assert_equals(tonumber(len), 5)
   assert_equals(val:tolua(), { 1, 2, 3, 4, 5 })
end

--- demux with out_len: slice a sub-vector out of a larger vector
--- (only out0 is read; out1 stays unconnected)
function TestMux:TestDemuxSlice()
   local b = launch("ubx/demux", { type = "double", nout = 2, out_len = { 3, 3 } },
		    "TestDemuxSlice")

   local p_in = ubx.port_clone_conn(b, "in", 1, nil, 7, 0)
   local p0 = ubx.port_clone_conn(b, "out0", nil, 1, 7, 0)
   ubx.block_tostate(b, 'active')

   -- a [p, v] state vector: extract the position part
   p_in:write({ 1.5, 2.5, 3.5, -1, -2, -3 })
   b:do_step()

   local len, val = p0:read()
   assert_equals(tonumber(len), 3)
   assert_equals(val:tolua(), { 1.5, 2.5, 3.5 })
end

--- out_len broadcast: nout=3, out_len=2 partitions a len-6 vector
function TestMux:TestDemuxLenBroadcast()
   local b = launch("ubx/demux", { type = "int32_t", nout = 3, out_len = 2 },
		    "TestDemuxBcast")

   local p_in = ubx.port_clone_conn(b, "in", 1, nil, 7, 0)
   local p_out = {}
   for i = 0, 2 do
      p_out[i] = ubx.port_clone_conn(b, "out" .. i, nil, 1, 7, 0)
   end
   ubx.block_tostate(b, 'active')

   p_in:write({ 1, 2, 3, 4, 5, 6 })
   b:do_step()

   local exp = { [0] = { 1, 2 }, [1] = { 3, 4 }, [2] = { 5, 6 } }
   for i = 0, 2 do
      local len, val = p_out[i]:read()
      assert_equals(tonumber(len), 2)
      assert_equals(val:tolua(), exp[i])
   end
end

--- invalid configs are refused at init
function TestMux:TestBadConfig()
   assert_true(not pcall(launch, "ubx/mux", { type = "no_such_type", nin = 2 },
			 "TestMuxBadType"))
   assert_true(not pcall(launch, "ubx/mux", { type = "double", nin = 0 },
			 "TestMuxBadN"))
   -- in_len array length must be 1 or nin
   assert_true(not pcall(launch, "ubx/mux",
			 { type = "double", nin = 3, in_len = { 1, 2 } },
			 "TestMuxBadLenLen"))
   -- sub-vector lengths must be >= 1
   assert_true(not pcall(launch, "ubx/demux",
			 { type = "double", nout = 2, out_len = { 1, 0 } },
			 "TestDemuxBadLen"))
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
