local luaunit = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")
local ffi = require("ffi")

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_ERR

local assert_true = luaunit.assert_true
local assert_equals = luaunit.assert_equals
local assert_not_equals = luaunit.assert_not_equals

TestFdtrig = {}

local MS = 1000000

local function ms(n) ubx.clock_mono_sleep(0, n * MS) end

local function now_ms()
   local ts = ubx.gettime()
   return tonumber(ts.sec) * 1e3 + tonumber(ts.nsec) / 1e6
end

local function drain(p)
   local res = {}
   while true do
      local cnt, v = p:read()
      if cnt <= 0 then return res end
      res[#res+1] = tonumber(v:tolua())
   end
end

local function last(p)
   local v = drain(p)
   return v[#v] or 0
end

-- a ramp in chain0 counts the steps
local function system(conf)
   conf.chain0 = { { b="#ramp" } }
   return bd.system {
      imports = { "stdtypes", "fdtrig", "ramp", "lfrb" },
      blocks = {
	 { name="ramp", type="ubx/ramp" },
	 { name="fdtrig", type="ubx/fdtrig" },
      },
      configurations = {
	 { name="ramp", config = { type_name="uint64_t", start=0, slope=1 } },
	 { name="fdtrig", config = conf },
      },
   }
end

-- launch, clone the ports to check and start
local function launch(name, conf, ports)
   local sys = system(conf)
   local nd = sys:launch{ nodename=name, nostart=true, loglevel=LOGLEVEL }
   local p = { steps = ubx.port_clone_conn(nd:b("ramp"), "out", 4096) }
   for _, pn in ipairs(ports or {}) do
      p[pn] = ubx.port_clone_conn(nd:b("fdtrig"), pn, 4096)
   end
   sys:startup(nd)
   return nd, p
end

local function sh(cmd)
   local r = os.execute(cmd)
   assert(r == 0 or r == true, cmd .. " failed")
end

local function tmpdir()
   local dir = os.tmpname()
   os.remove(dir)
   sh("mkdir " .. dir)
   return dir
end

local function mkfifo()
   local dir = tmpdir()
   sh("mkfifo " .. dir .. "/fifo")
   return dir, dir .. "/fifo"
end

local function rmdir(dir, file)
   os.remove(dir .. "/" .. file)
   os.remove(dir)
end

-- returns the do_stop result and its duration
local function timed_stop(b)
   local t0 = now_ms()
   local ret = b:do_stop()
   return ret, now_ms() - t0
end

function TestFdtrig:TestTimerfd()
   local nd, p = launch("TestTimerfd",
			{ source="timerfd", period_ns=10*MS },
			{ "event_cnt", "coalesced_cnt" })
   ms(300)
   local b = nd:b("fdtrig")
   assert_equals(b:do_stop(), 0)
   local steps = #drain(p.steps)
   local events = last(p.event_cnt)
   local coalesced = last(p.coalesced_cnt)
   ubx.node_rm(nd)

   assert_true(events >= 20 and events <= 32, "events: " .. events)
   assert_equals(steps, events)
   assert_true(coalesced <= 2, "coalesced: " .. coalesced)
end

function TestFdtrig:TestPathFifo()
   local dir, fifo = mkfifo()
   local nd, p = launch("TestPathFifo",
			{ source="path", path=fifo, read_size=1 },
			{ "event_cnt", "coalesced_cnt" })
   -- the block holds the read end, so this open does not block
   local w = assert(io.open(fifo, "w"))

   for _ = 1, 5 do
      w:write("x"); w:flush()
      ms(20)
   end
   local steps1 = #drain(p.steps)
   local events1 = last(p.event_cnt)

   -- one wakeup, 10 reads of read_size 1
   w:write("0123456789"); w:flush()
   ms(50)
   local steps2 = #drain(p.steps)
   local events2 = last(p.event_cnt)
   local coalesced = last(p.coalesced_cnt)

   -- hangup stops the block instead of spinning on POLLHUP
   w:close()
   ms(50)
   local b = nd:b("fdtrig")
   local state = b.block_state
   local steps3 = #drain(p.steps)
   ubx.node_rm(nd)
   rmdir(dir, "fifo")

   assert_equals(steps1, 5)
   assert_equals(events1, 5)
   assert_equals(steps2, 1)
   assert_equals(events2, 6)
   assert_equals(coalesced, 9)
   assert_equals(state, ffi.C.BLOCK_STATE_INACTIVE)
   assert_equals(steps3, 0)
end

-- start reopens the path, so a new writer is served after a hangup
function TestFdtrig:TestPathFifoRestart()
   local dir, fifo = mkfifo()
   local nd, p = launch("TestPathFifoRestart",
			{ source="path", path=fifo, read_size=1 })
   local b = nd:b("fdtrig")
   local res = {}

   for run = 1, 2 do
      local w = assert(io.open(fifo, "w"))
      for _ = 1, 3 do
	 w:write("x"); w:flush()
	 ms(20)
      end
      w:close()
      ms(50)
      res[run] = { steps=#drain(p.steps), state=b.block_state }
      if run == 1 then res.start = b:do_start() end
   end
   ubx.node_rm(nd)
   rmdir(dir, "fifo")

   for run = 1, 2 do
      assert_equals(res[run].steps, 3, "run " .. run)
      assert_equals(res[run].state, ffi.C.BLOCK_STATE_INACTIVE, "run " .. run)
   end
   assert_equals(res.start, 0)
end

-- data present at start is dropped, then EOF stops the block
function TestFdtrig:TestPathStaleDropped()
   local dir = tmpdir()
   local f = assert(io.open(dir .. "/data", "w"))
   f:write("0123456789"); f:close()

   local nd, p = launch("TestPathStaleDropped",
			{ source="path", path=dir .. "/data", read_size=1 },
			{ "event_cnt" })
   ms(50)
   local state = nd:b("fdtrig").block_state
   local steps = #drain(p.steps)
   local events = #drain(p.event_cnt)
   ubx.node_rm(nd)
   rmdir(dir, "data")

   assert_equals(steps, 0)
   assert_equals(events, 0)
   assert_equals(state, ffi.C.BLOCK_STATE_INACTIVE)
end

-- a source that never runs dry must not hang start, stop or cleanup
function TestFdtrig:TestPathDevZero()
   local t0 = now_ms()
   local nd, p = launch("TestPathDevZero",
			{ source="path", path="/dev/zero", read_size=64 },
			{ "coalesced_cnt" })
   local t_start = now_ms() - t0
   ms(50)
   local ret, t_stop = timed_stop(nd:b("fdtrig"))
   local steps = #drain(p.steps)
   local coalesced = last(p.coalesced_cnt)
   t0 = now_ms()
   ubx.node_rm(nd)
   local t_rm = now_ms() - t0

   assert_equals(ret, 0)
   assert_true(t_start < 500, "start took " .. t_start .. " ms")
   assert_true(t_stop < 200, "stop took " .. t_stop .. " ms")
   assert_true(t_rm < 500, "node_rm took " .. t_rm .. " ms")
   assert_true(steps > 0)
   assert_true(coalesced >= steps * 62, "coalesced " .. coalesced .. ", steps " .. steps)
end

-- the eventfd wakes a thread blocked in poll without a timeout, and
-- stop returns once it parked
function TestFdtrig:TestStopWakesPoll()
   local nd = launch("TestStopWakesPoll",
		     { source="timerfd", period_ns=10000*MS })
   ms(20)
   local ret, t_stop = timed_stop(nd:b("fdtrig"))
   ubx.node_rm(nd)

   assert_equals(ret, 0)
   assert_true(t_stop < 40, "stop took " .. t_stop .. " ms")
end

local function run_timeouts(name, timeout_step)
   -- the timer never fires within the run
   local nd, p = launch(name,
			{ source="timerfd", period_ns=10000*MS,
			  timeout_ns=10*MS, timeout_step=timeout_step },
			{ "timeout_cnt", "event_cnt" })
   ms(200)
   assert_equals(nd:b("fdtrig"):do_stop(), 0)
   local res = {
      steps = #drain(p.steps),
      timeouts = last(p.timeout_cnt),
      events = #drain(p.event_cnt),
   }
   ubx.node_rm(nd)
   return res
end

function TestFdtrig:TestTimeoutCount()
   local r = run_timeouts("TestTimeoutCount", 0)
   assert_true(r.timeouts >= 10 and r.timeouts <= 21, "timeouts: " .. r.timeouts)
   assert_equals(r.steps, 0)
   assert_equals(r.events, 0)
end

function TestFdtrig:TestTimeoutStep()
   local r = run_timeouts("TestTimeoutStep", 1)
   assert_true(r.timeouts >= 10 and r.timeouts <= 21, "timeouts: " .. r.timeouts)
   assert_equals(r.steps, r.timeouts)
   assert_equals(r.events, 0)
end

function TestFdtrig:TestStartStopCycles()
   local nd, p = launch("TestStartStopCycles",
			{ source="timerfd", period_ns=1*MS })
   local b = nd:b("fdtrig")

   for _ = 1, 3 do
      ms(30)
      assert_equals(b:do_stop(), 0)
      assert_true(#drain(p.steps) > 0)
      ms(30)
      assert_equals(#drain(p.steps), 0, "stepped while stopped")
      assert_equals(b:do_start(), 0)
   end

   ubx.node_rm(nd)
end

-- a restart right after a self-stop waits for the thread to park
function TestFdtrig:TestAutostopRestart()
   local nd, p = launch("TestAutostopRestart",
			{ source="timerfd", period_ns=1*MS, autostop_steps=5 })
   local b = nd:b("fdtrig")
   ms(100)
   local state = b.block_state
   local steps = #drain(p.steps)
   local ret = b:do_start()
   ms(50)
   local steps2 = #drain(p.steps)
   ubx.node_rm(nd)

   assert_equals(steps, 5)
   assert_equals(state, ffi.C.BLOCK_STATE_INACTIVE)
   assert_equals(ret, 0)
   -- autostop_steps is used up: the restart runs on
   assert_true(steps2 > 5, "steps after restart: " .. steps2)
end

-- returns the block_init result
local function init_with(name, conf)
   local nd = ubx.node_create(name, { loglevel=ffi.C.UBX_LOGLEVEL_CRIT })
   ubx.load_module(nd, "stdtypes")
   ubx.load_module(nd, "fdtrig")
   local b = ubx.block_create(nd, "ubx/fdtrig", "fdtrig", conf)
   local ret = ubx.block_init(b)
   local state = b.block_state
   ubx.node_rm(nd)
   return ret, state
end

function TestFdtrig:TestInvalidConfig()
   local cases = {
      nosource = { },
      badsource = { source="socket" },
      deadline = { source="timerfd", period_ns=1*MS,
		   sched_policy="SCHED_DEADLINE" },
      noperiod = { source="timerfd" },
      zeroperiod = { source="timerfd", period_ns=0 },
      negperiod = { source="timerfd", period_ns=-1 },
      negtimeout = { source="timerfd", period_ns=1*MS, timeout_ns=-1 },
      nopath = { source="path" },
      badpath = { source="path", path="/nonexistent/fdtrig" },
      longpath = { source="path", path="/" .. string.rep("x", 5000) },
      zeroreadsize = { source="path", path="/dev/zero", read_size=0 },
      nochip = { source="gpio", gpio_line=0 },
      noline = { source="gpio", gpio_chip="gpiochip0" },
      missingchip = { source="gpio", gpio_chip="gpiochip_nonexistent", gpio_line=0 },
      badedge = { source="gpio", gpio_chip="gpiochip0", gpio_line=0, gpio_edge="up" },
      badtimeoutstep = { source="timerfd", period_ns=1*MS, timeout_step=2 },
   }
   for name, conf in pairs(cases) do
      local ret, state = init_with("TestInvalidConfig_" .. name, conf)
      assert_not_equals(ret, 0, name .. ": init must fail")
      assert_equals(state, ffi.C.BLOCK_STATE_PREINIT, name)
   end
end

function TestFdtrig:TestValidConfigInits()
   local ret = init_with("TestValidConfigInits",
			 { source="timerfd", period_ns=1*MS, timeout_ns=0,
			   sched_policy="SCHED_OTHER" })
   assert_equals(ret, 0)
end

if not _RUNNER then os.exit( luaunit.LuaUnit.run() ) end
