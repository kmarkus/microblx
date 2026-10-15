--
-- Stress test for hot connect/disconnect (requires ENABLE_URCU).
--
-- HOTC_TRIG ptrig threads each step a ramp writer w<i> and a checker
-- luablock c<i>. While they run:
--
--  - the main thread randomly connects/disconnects w<i>.out -> c<j>.in
--    via ubx.connect/ubx.disconnect, i.e. creating, starting, stopping
--    and removing an lfrb for every connection.
--  - a self-triggered luablock thread (rewirer) concurrently toggles
--    preallocated lfrbs x<k> between w<i>.out and c<j>.in2 with the
--    raw C API, so both control threads modify the same out arrays.
--
-- Writer w<i> emits i * 2^40 + seq. The checkers verify that every
-- value read decodes to a valid writer, so reading through a stale
-- array or a freed iblock shows up as errors, if ASan does not catch
-- it first. Scale it up with e.g.
--
--   HOTC_ITER=200000 HOTC_TRIG=4 luajit tests/test_hot_connect.lua
--
-- and run it under ASan (LD_PRELOAD=libasan.so) for the brutal version.
--
-- HOTC_MEASURE=1 and the knobs below turn it into a latency test of an
-- RT trigger (trig1) that is not rewired itself, see
-- docs/dev/008-urcu-runtime-reconfig.md.
--

local lu = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")
local ffi = require("ffi")

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_WARN

local ITER = tonumber(os.getenv("HOTC_ITER") or 3000)
local NTRIG = tonumber(os.getenv("HOTC_TRIG") or 3)
local NX = tonumber(os.getenv("HOTC_NX") or 4)		-- rewirer lfrbs
local MAXC = tonumber(os.getenv("HOTC_MAXCONN") or 16)	-- main thread conns
local PERIOD_US = tonumber(os.getenv("HOTC_PERIOD_US") or 100)
local SEED = tonumber(os.getenv("HOTC_SEED") or os.time())

-- latency measurement knobs
local SEC = tonumber(os.getenv("HOTC_SEC"))		-- run for N s instead of HOTC_ITER
local IDLE = os.getenv("HOTC_IDLE")			-- main thread does not rewire
local RT_PRIO = tonumber(os.getenv("HOTC_RT_PRIO"))	-- SCHED_FIFO prio of the ptrigs
local CPU = tonumber(os.getenv("HOTC_CPU"))		-- pin the ptrigs to this CPU
local NOMB = os.getenv("HOTC_NOMB")			-- liburcu-bp without membarrier
local RATE = tonumber(os.getenv("HOTC_RATE"))		-- main thread ops/s (def: max)
-- HOTC_MEASURE=1: trig1 is only measured: static w1 -> c1 connection,
-- RT/CPU/stats settings apply to it alone, rewiring uses trig2..N
local MEASURE = os.getenv("HOTC_MEASURE")
-- trig1 load in measure mode: C only, ramp_double -> math_double (sin)
-- on vectors of this length
local LOAD_N = tonumber(os.getenv("HOTC_LOAD_N") or 4000)
local FIRST = MEASURE and 2 or 1			-- first rewired trigger

local IBCONF = { buffer_len = 4, loglevel_overruns = -1 }

local checker = [[
local ubx = require("ubx")
local NSRC = %d
local SRC = 2^40
local ports, sample, p_stat
local nread, nerr = 0, 0

function init(b)
   ubx.inport_add(b, "in", "from main thread conns", 0, "uint64_t", 1)
   ubx.inport_add(b, "in2", "from rewirer conns", 0, "uint64_t", 1)
   ubx.outport_add(b, "stat", "{reads, errors}", 0, "uint64_t", 2)
   ports = { ubx.port_get(b, "in"), ubx.port_get(b, "in2") }
   p_stat = ubx.port_get(b, "stat")
   sample = ubx.port_alloc_read_sample(ports[1])
   return true
end

function step(b)
   for _, p in ipairs(ports) do
      for _ = 1, 64 do
	 local len = ubx.port_read(p, sample)
	 if len <= 0 then break end
	 local v = tonumber(sample:tolua())
	 local src = math.floor(v / SRC)
	 if src < 1 or src > NSRC or v %% SRC > 1e12 then nerr = nerr + 1 end
	 nread = nread + 1
      end
   end
   ubx.port_write(p_stat, { nread, nerr })
end

function cleanup(b)
   ubx.port_rm(b, "in")
   ubx.port_rm(b, "in2")
   ubx.port_rm(b, "stat")
end
]]

local rewirer = [[
local ubx = require("ubx")
local ffi = require("ffi")
local C = ubx.ubx
local FIRST, NTRIG, NX, SEED = %d, %d, %d, %d
local wout, cin, xs, conn = {}, {}, {}, {}
local p_stat
local nops, nerr = 0, 0

function init(b)
   local nd = ffi.cast("ubx_block_t*", b).nd
   for i = FIRST, NTRIG do
      wout[i] = ubx.port_get(ubx.block_get(nd, "w"..i), "out")
      cin[i] = ubx.port_get(ubx.block_get(nd, "c"..i), "in2")
   end
   for k = 1, NX do xs[k] = ubx.block_get(nd, "x"..k) end
   ubx.outport_add(b, "stat", "{ops, errors}", 0, "uint64_t", 2)
   p_stat = ubx.port_get(b, "stat")
   math.randomseed(SEED)
   return true
end

local function chk(ret) if ret ~= 0 then nerr = nerr + 1 end end

local function toggle(k)
   local c = conn[k]
   if c then
      chk(C.ubx_port_disconnect_out(c[1], xs[k]))
      chk(C.ubx_port_disconnect_in(c[2], xs[k]))
      conn[k] = nil
   else
      c = { wout[math.random(FIRST, NTRIG)], cin[math.random(FIRST, NTRIG)] }
      chk(C.ubx_port_connect_out(c[1], xs[k]))
      chk(C.ubx_port_connect_in(c[2], xs[k]))
      conn[k] = c
   end
   nops = nops + 1
end

function step(b)
   toggle(math.random(NX))
   ubx.port_write(p_stat, { nops, nerr })
end

function stop(b)
   for k = 1, NX do if conn[k] then toggle(k) end end
   ubx.port_write(p_stat, { nops, nerr })
end

function cleanup(b) ubx.port_rm(b, "stat") end
]]

local function fill(v, n) local t = {} for i = 1, n do t[i] = v end return t end

local function build_sys()
   local blocks, configs, chains = {}, {}, {}

   for i = 1, NTRIG do
      if MEASURE and i == 1 then
	 blocks[#blocks+1] = { name = "w1", type = "ubx/ramp_double" }
	 blocks[#blocks+1] = { name = "c1", type = "ubx/math_double" }
      else
	 blocks[#blocks+1] = { name = "w"..i, type = "ubx/ramp_uint64" }
	 blocks[#blocks+1] = { name = "c"..i, type = "ubx/luablock" }
      end
      blocks[#blocks+1] = { name = "trig"..i, type = "ubx/ptrig" }
      if MEASURE and i == 1 then
	 configs[#configs+1] = { name = "w1", config = { slope = fill(1, LOAD_N), data_len = LOAD_N } }
	 configs[#configs+1] = { name = "c1", config = { func = "sin", data_len = LOAD_N } }
      else
	 configs[#configs+1] = { name = "w"..i, config = { start = i * 2^40, slope = 1 } }
	 configs[#configs+1] = { name = "c"..i, config = { lua_str = string.format(checker, NTRIG) } }
      end
      local tc = { period = { sec = 0, usec = PERIOD_US },
		   chain0 = { { b = "#w"..i }, { b = "#c"..i } },
		   latency_stats = 1, tstats_mode = 1, tstats_skip_first = 10,
		   loglevel = ffi.C.UBX_LOGLEVEL_INFO }
      if not MEASURE or i == 1 then
	 if RT_PRIO then tc.sched_policy = "SCHED_FIFO"; tc.sched_priority = RT_PRIO end
	 if CPU then tc.affinity = { CPU } end
      end
      configs[#configs+1] = { name = "trig"..i, config = tc }
   end

   for k = 1, NX do
      blocks[#blocks+1] = { name = "x"..k, type = "ubx/lfrb" }
      configs[#configs+1] = { name = "x"..k, config = {
				 type_name = "uint64_t", buffer_len = 4,
				 loglevel_overruns = -1 } }
   end

   -- rewirer is created after the start, see below
   return bd.system {
      imports = { "stdtypes", "ptrig", "lfrb", "ramp_uint64", "luablock",
		  "ramp_double", "math_double" },
      blocks = blocks,
      configurations = configs,
      connections = MEASURE and { { src = "w1.out", tgt = "c1.x", config = { buffer_len = 1 } } } or nil,
   }
end

-- return the last {a, b} sample from a stat port clone
local function last_stat(p)
   local res
   while true do
      local len, s = p:read()
      if len <= 0 then break end
      res = s:tolua()
   end
   return { tonumber(res[1]), tonumber(res[2]) }
end

TestHotConnect = {}

function TestHotConnect:TestStress()
   if not ubx.hot_connect() then lu.skip("libubx built without ENABLE_URCU") end

   if NOMB then
      -- undocumented: bp falls back to smp_mb when this is 0. Only
      -- safe before any thread is inside a read section.
      ffi.cdef("extern int urcu_bp_has_sys_membarrier;")
      ffi.load("liburcu-bp.so.8", true).urcu_bp_has_sys_membarrier = 0
   end

   math.randomseed(SEED)
   print(string.format("\nhot connect: iter %d, triggers %d, rewirer lfrbs %d, seed %d",
		       ITER, NTRIG, NX, SEED))

   local sys = build_sys()
   local nd = sys:launch { nostart = true, loglevel = LOGLEVEL, nodename = 'hotc' }

   local p_cstat = {}
   for i = FIRST, NTRIG do
      p_cstat[i] = ubx.port_clone_conn(nd:b("c"..i), "stat", 1)
   end

   local rw, p_rwstat
   if NX > 0 then
      rw = ubx.block_create(nd, "ubx/luablock", "rewirer", {
			       lua_str = string.format(rewirer, FIRST, NTRIG, NX, SEED),
			       thread = 1, period = 1 })
      ubx.block_tostate(rw, 'inactive')
      p_rwstat = ubx.port_clone_conn(rw, "stat", 1)
   end

   local _, nib_before = ubx.num_blocks(nd)

   sys:startup(nd)
   if rw then ubx.block_tostate(rw, 'active') end

   local conns = {}	-- { {wi, cj}, ... }
   local nconn, ndisc = 0, 0
   local t0 = ubx.gettime()

   local it = 0
   local function running()
      it = it + 1
      if SEC then return ubx.gettime() - t0 < SEC end
      return it <= ITER
   end

   while running() do
      if RATE then ubx.nanosleep(0, math.floor(1e9 / RATE)) end
      if IDLE then
	 ubx.nanosleep(0, 10000000)
      elseif #conns < MAXC and (#conns == 0 or math.random() < 0.55) then
	 local wi, cj = math.random(FIRST, NTRIG), math.random(FIRST, NTRIG)
	 local ok, e = ubx.connect(nd, "w"..wi, "out", "c"..cj, "in", "ubx/lfrb", IBCONF)
	 lu.assert_true(ok, e)
	 conns[#conns+1] = { wi, cj }
	 nconn = nconn + 1
      else
	 local c = conns[math.random(#conns)]
	 local ok, e = ubx.disconnect(nd, "w"..c[1], "out", "c"..c[2], "in")
	 lu.assert_true(ok, e)
	 -- disconnect removes all iblocks between the pair
	 for i = #conns, 1, -1 do
	    if conns[i][1] == c[1] and conns[i][2] == c[2] then table.remove(conns, i) end
	 end
	 ndisc = ndisc + 1
      end
   end

   -- tear down the remaining main thread connections while running
   while #conns > 0 do
      local c = conns[1]
      lu.assert_true(ubx.disconnect(nd, "w"..c[1], "out", "c"..c[2], "in"))
      for i = #conns, 1, -1 do
	 if conns[i][1] == c[1] and conns[i][2] == c[2] then table.remove(conns, i) end
      end
   end

   if rw then ubx.block_tostate(rw, 'inactive') end
   for i = 1, NTRIG do nd:b("trig"..i):do_stop() end

   local rws = rw and last_stat(p_rwstat) or { 0, 0 }
   local dur = ubx.gettime() - t0
   print(string.format("main: %d connects, %d disconnects in %.1fs; rewirer: %d ops, %d errors",
		       nconn, ndisc, dur, rws[1], rws[2]))
   if rw then lu.assert_true(rws[1] > 0, "rewirer did not run") end
   lu.assert_equals(rws[2], 0, "rewirer connect/disconnect errors")

   for i = FIRST, NTRIG do
      local s = last_stat(p_cstat[i])
      print(string.format("c%d: %d reads, %d errors", i, s[1], s[2]))
      if not IDLE or rw then lu.assert_true(s[1] > 0, "checker c"..i.." read nothing") end
      lu.assert_equals(s[2], 0, "checker c"..i.." read invalid values")

      local cc = ubx.port_conns_totab(ubx.port_get(nd:b("c"..i), "in"))
      lu.assert_equals(#cc.incoming, 0, "c"..i..".in still connected")
      cc = ubx.port_conns_totab(ubx.port_get(nd:b("c"..i), "in2"))
      lu.assert_equals(#cc.incoming, 0, "c"..i..".in2 still connected")
   end

   local _, nib = ubx.num_blocks(nd)
   lu.assert_equals(nib, nib_before, "leaked iblocks")

   ubx.node_rm(nd)
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
