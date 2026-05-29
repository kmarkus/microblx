#!/usr/bin/luajit
--
-- Test the udpsink luablock: dynamic port creation from the `ports`
-- lua_str config, JSON-over-UDP output, and the two timestamp modes
-- (auto monotonic vs. a connected "ts" input port).
--

local lu = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")
local ffi = require("ffi")

local has_json, json = pcall(require, "cjson")
if not has_json then has_json, json = pcall(require, "json") end
if not has_json then
   io.stderr:write("WARNING: test_udpsink: skipping (no cjson or json module found)\n")
   return
end

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_WARN

-- locate the source udpsink.lua relative to this test file
local here = (debug.getinfo(1, "S").source:gsub("^@", "")):match("(.*/)") or "./"
local UDPSINK_LUA = here .. "../std_blocks/udpsink/udpsink.lua"

-- minimal FFI UDP receiver (independent cdefs; the block runs in its
-- own lua_State so there is no clash)
ffi.cdef[[
int socket(int domain, int type, int protocol);
int bind(int fd, const void *addr, unsigned int addrlen);
long recvfrom(int fd, void *buf, unsigned long len, int flags,
	      void *src_addr, unsigned int *addrlen);
int close(int fd);
int setsockopt(int fd, int level, int optname, const void *optval, unsigned int optlen);
unsigned short htons(unsigned short hostshort);
struct t_in_addr { uint32_t s_addr; };
struct t_sockaddr_in {
	unsigned short sin_family;
	unsigned short sin_port;
	struct t_in_addr sin_addr;
	char sin_zero[8];
};
struct t_timeval { long tv_sec; long tv_usec; };
]]

local AF_INET     = 2
local SOCK_DGRAM  = 2
local SOL_SOCKET  = 1
local SO_REUSEADDR = 2
local SO_RCVTIMEO = 20

local function make_receiver(udp_port)
   local fd = ffi.C.socket(AF_INET, SOCK_DGRAM, 0)
   assert(fd >= 0, "receiver socket() failed")
   local one = ffi.new("int[1]", 1)
   ffi.C.setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, one, ffi.sizeof("int"))
   -- 1s receive timeout so a missing datagram fails the test instead of hanging
   local tv = ffi.new("struct t_timeval"); tv.tv_sec = 1; tv.tv_usec = 0
   ffi.C.setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, tv, ffi.sizeof("struct t_timeval"))
   local addr = ffi.new("struct t_sockaddr_in")
   addr.sin_family = AF_INET
   addr.sin_port = ffi.C.htons(udp_port)
   addr.sin_addr.s_addr = 0 -- INADDR_ANY
   assert(ffi.C.bind(fd, addr, ffi.sizeof(addr)) == 0, "receiver bind failed")
   return fd
end

local function recv_json(fd)
   local buf = ffi.new("char[?]", 2048)
   local n = ffi.C.recvfrom(fd, buf, 2048, 0, nil, nil)
   if n <= 0 then return nil end
   return json.decode(ffi.string(buf, n))
end

local ni
local next_port = 45460

local function launch_sink(ports_decl)
   local udp_port = next_port; next_port = next_port + 1
   local lua_str = ports_decl .. ("\nhost='127.0.0.1'\nport=%d\n"):format(udp_port)

   local sys = bd.system {
      imports = { "stdtypes", "lfrb" },
      blocks = {
	 { name = "sink", type = "luablock:" .. UDPSINK_LUA },
      },
      configurations = {
	 { name = "sink", config = { lua_str = lua_str } },
      },
   }

   ni = sys:launch({ nodename = "TestUdpsink", nostart = true, loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)
   return ni:b("sink"), udp_port
end

TestUdpsink = {}

function TestUdpsink:teardown()
   if ni then ubx.node_rm(ni) end
   ni = nil
end

--- ports are created from the lua_str `ports` table with the right types
function TestUdpsink:Test_ports_created()
   local sink = launch_sink("ports = { x='double', n='int32_t' }")
   local px = ubx.block_port_get(sink, "x")
   local pn = ubx.block_port_get(sink, "n")
   lu.assert_not_nil(px)
   lu.assert_not_nil(pn)
   lu.assert_equals(ffi.string(px.in_type.name), "double")
   lu.assert_equals(ffi.string(pn.in_type.name), "int32_t")
   lu.assert_false(pcall(ubx.block_port_get, sink, "nope")) -- absent port
end

--- a connected "ts" port overrides the auto timestamp
function TestUdpsink:Test_ts_from_port()
   local sink, udp_port = launch_sink("ports = { ts='double', a='double' }")
   local rx = make_receiver(udp_port)

   local p_ts = ubx.port_clone_conn(sink, "ts", 1, nil, 7, 0)
   local p_a  = ubx.port_clone_conn(sink, "a",  1, nil, 7, 0)
   ubx.block_tostate(sink, 'active')

   p_ts:write(1.5)
   p_a:write(42.0)
   sink:do_step()

   local o = recv_json(rx)
   ffi.C.close(rx)

   lu.assert_not_nil(o)
   lu.assert_almost_equals(o.ts, 1.5, 1e-9) -- from the port, not the clock
   lu.assert_almost_equals(o.a, 42.0, 1e-9)
end

--- without a "ts" port, a monotonic timestamp is injected
function TestUdpsink:Test_auto_ts()
   local sink, udp_port = launch_sink("ports = { a='double' }")
   local rx = make_receiver(udp_port)

   local p_a = ubx.port_clone_conn(sink, "a", 1, nil, 7, 0)
   ubx.block_tostate(sink, 'active')

   p_a:write(7.0);  sink:do_step()
   local o1 = recv_json(rx)
   p_a:write(8.0);  sink:do_step()
   local o2 = recv_json(rx)
   ffi.C.close(rx)

   lu.assert_not_nil(o1)
   lu.assert_not_nil(o2)
   lu.assert_almost_equals(o1.a, 7.0, 1e-9)
   lu.assert_almost_equals(o2.a, 8.0, 1e-9)
   lu.assert_not_nil(o1.ts)        -- auto ts present
   lu.assert_true(o2.ts >= o1.ts)  -- monotonic
end

--- a single step drains ALL buffered samples (one datagram each, in
--- order) -- this is what lets a slow/decoupled sink not lose samples
function TestUdpsink:Test_drains_all_buffered()
   local sink, udp_port = launch_sink("ports = { ts='double', a='double' }")
   local rx = make_receiver(udp_port)

   local N = 8
   -- buff_len1 (3rd arg) sizes the producer->sink connection buffer
   local p_ts = ubx.port_clone_conn(sink, "ts", 16, nil, 7, 0)
   local p_a  = ubx.port_clone_conn(sink, "a",  16, nil, 7, 0)
   ubx.block_tostate(sink, 'active')

   for i = 1, N do
      p_ts:write(i + 0.0)
      p_a:write(i * 10.0)
   end
   sink:do_step() -- one step must emit all N buffered frames

   for i = 1, N do
      local o = recv_json(rx)
      lu.assert_not_nil(o, "missing datagram " .. i)
      lu.assert_almost_equals(o.ts, i + 0.0, 1e-9)   -- FIFO order preserved
      lu.assert_almost_equals(o.a, i * 10.0, 1e-9)
   end
   local extra = recv_json(rx) -- nothing beyond the N buffered (times out)
   ffi.C.close(rx)
   lu.assert_nil(extra)
end

--- a port lagging by one step (producer caught mid-cycle) is waited for,
--- then emitted complete and ALIGNED -- not as a partial frame
function TestUdpsink:Test_grace_aligns_lagging_port()
   local sink, udp_port = launch_sink("ports = { ts='double', a='double' }")
   local rx = make_receiver(udp_port)

   local p_ts = ubx.port_clone_conn(sink, "ts", 16, nil, 7, 0)
   local p_a  = ubx.port_clone_conn(sink, "a",  16, nil, 7, 0)
   ubx.block_tostate(sink, 'active')

   p_ts:write(1.0)         -- only "ts" updated this step
   sink:do_step()
   lu.assert_nil(recv_json(rx)) -- held during grace, nothing sent yet

   p_a:write(10.0)         -- the lagging "a" arrives next step
   sink:do_step()
   local o = recv_json(rx)
   ffi.C.close(rx)
   lu.assert_not_nil(o)
   lu.assert_almost_equals(o.ts, 1.0, 1e-9)   -- ts[1] paired with a[1]
   lu.assert_almost_equals(o.a, 10.0, 1e-9)
end

--- a port that stops producing is dropped after the grace step; the
--- other ports keep streaming at full rate (no stall, no permanent skew)
function TestUdpsink:Test_stopped_port_dropped_after_grace()
   local sink, udp_port = launch_sink("ports = { ts='double', a='double' }")
   local rx = make_receiver(udp_port)

   local p_ts = ubx.port_clone_conn(sink, "ts", 16, nil, 7, 0)
   local p_a  = ubx.port_clone_conn(sink, "a",  16, nil, 7, 0)
   ubx.block_tostate(sink, 'active')

   p_ts:write(1.0); p_a:write(10.0)  -- warm up: one complete frame
   sink:do_step()
   local o1 = recv_json(rx)
   lu.assert_not_nil(o1)
   lu.assert_almost_equals(o1.a, 10.0, 1e-9)

   p_ts:write(2.0)                   -- "a" stops; ts keeps going
   sink:do_step()
   lu.assert_nil(recv_json(rx))      -- held one step (grace)

   p_ts:write(3.0)                   -- a still absent -> grace expired
   sink:do_step()
   local o2 = recv_json(rx)
   local o3 = recv_json(rx)
   ffi.C.close(rx)
   lu.assert_not_nil(o2)
   lu.assert_almost_equals(o2.ts, 2.0, 1e-9)
   lu.assert_nil(o2.a)               -- dropped, not stalled
   lu.assert_not_nil(o3)
   lu.assert_almost_equals(o3.ts, 3.0, 1e-9)
   lu.assert_nil(o3.a)
end

--- no datagram is sent when no port produced data
function TestUdpsink:Test_no_data_no_send()
   local sink, udp_port = launch_sink("ports = { a='double' }")
   local rx = make_receiver(udp_port)

   ubx.port_clone_conn(sink, "a", 1, nil, 7, 0)
   ubx.block_tostate(sink, 'active')

   sink:do_step() -- nothing written -> nothing sent
   local o = recv_json(rx) -- should time out (1s)
   ffi.C.close(rx)
   lu.assert_nil(o)
end

--- an unknown type in `ports` fails block init
function TestUdpsink:Test_bad_type_fails()
   local ok = pcall(launch_sink, "ports = { a='no_such_type' }")
   lu.assert_false(ok)
end

if not _RUNNER then
   os.exit(lu.LuaUnit.run())
end
