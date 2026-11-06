#!/usr/bin/luajit
--
-- Test the netsink luablock: dynamic port creation from the `ports`
-- lua_str config, the real (config_add) runtime settings, and both
-- encodings (JSON and MessagePack) over UDP, and the ZeroMQ framing.
-- The subscriber joins late, so the zmq tests write until it receives.
--

local lu = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")
local ffi = require("ffi")

local has_json, json = pcall(require, "cjson")
if not has_json then has_json, json = pcall(require, "json") end
if not has_json then
   io.stderr:write("WARNING: test_netsink: skipping (no cjson or json module found)\n")
   return
end

local has_mp, mp = pcall(require, "MessagePack")
if not has_mp then has_mp, mp = pcall(require, "messagepack") end
if not has_mp then has_mp, mp = pcall(require, "cmsgpack") end

local LOGLEVEL = ffi.C.UBX_LOGLEVEL_WARN

-- locate the source netsink.lua relative to this test file; on
-- installed systems (no source tree) fall back to resolving it via
-- the block search path (e.g. /usr/share/ubx/blocks/<ver>/)
local here = (debug.getinfo(1, "S").source:gsub("^@", "")):match("(.*/)") or "./"
local NETSINK_LUA = here .. "../std_blocks/netsink/netsink.lua"

do
   local f = io.open(NETSINK_LUA)
   if f then f:close() else NETSINK_LUA = "netsink" end
end

-- minimal FFI UDP receiver (independent cdefs; the block runs in its own
-- lua_State so there is no clash)
ffi.cdef[[
int socket(int domain, int type, int protocol);
int bind(int fd, const void *addr, unsigned int addrlen);
long recvfrom(int fd, void *buf, unsigned long len, int flags,
	      void *src_addr, unsigned int *addrlen);
int close(int fd);
int getpid(void);
int setsockopt(int fd, int level, int optname, const void *optval, unsigned int optlen);
unsigned short htons(unsigned short hostshort);
struct nt_in_addr { uint32_t s_addr; };
struct nt_sockaddr_in {
	unsigned short sin_family;
	unsigned short sin_port;
	struct nt_in_addr sin_addr;
	char sin_zero[8];
};
struct nt_timeval { long tv_sec; long tv_usec; };
]]

local AF_INET      = 2
local SOCK_DGRAM   = 2
local SOL_SOCKET   = 1
local SO_REUSEADDR = 2
local SO_RCVTIMEO  = 20

local function make_receiver(udp_port)
   local fd = ffi.C.socket(AF_INET, SOCK_DGRAM, 0)
   assert(fd >= 0, "receiver socket() failed")
   local one = ffi.new("int[1]", 1)
   ffi.C.setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, one, ffi.sizeof("int"))
   local tv = ffi.new("struct nt_timeval"); tv.tv_sec = 1; tv.tv_usec = 0
   ffi.C.setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, tv, ffi.sizeof("struct nt_timeval"))
   local addr = ffi.new("struct nt_sockaddr_in")
   addr.sin_family = AF_INET
   addr.sin_port = ffi.C.htons(udp_port)
   addr.sin_addr.s_addr = 0 -- INADDR_ANY
   assert(ffi.C.bind(fd, addr, ffi.sizeof(addr)) == 0, "receiver bind failed")
   return fd
end

-- receive one datagram and decode it with the given format's decoder
local function recv(fd, decode)
   local buf = ffi.new("char[?]", 2048)
   local n = ffi.C.recvfrom(fd, buf, 2048, 0, nil, nil)
   if n <= 0 then return nil end
   return decode(ffi.string(buf, n))
end

local ni
local next_port = 45480

-- launch a netsink; all settings go through lua_str globals. `extra` is
-- extra lua_str appended after the ports/host/port declarations (e.g.
-- "format='msgpack'") so a test can select transport/format.
local function launch_sink(ports_decl, extra)
   local udp_port = next_port; next_port = next_port + 1
   local lua_str = ports_decl ..
      ("\nhost='127.0.0.1'\nport=%d\n"):format(udp_port) ..
      (extra or "")

   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "ptrig" },
      blocks = {
	 { name = "sink", type = "luablock:" .. NETSINK_LUA },
      },
      configurations = {
	 { name = "sink", config = { lua_str = lua_str } },
      },
   }

   ni = sys:launch({ nodename = "TestNetsink", nostart = true, loglevel = LOGLEVEL })
   lu.assert_not_nil(ni)
   return ni:b("sink"), udp_port
end

TestNetsink = {}

function TestNetsink:teardown()
   if ni then ubx.node_rm(ni) end
   ni = nil
   os.execute("rm -f /tmp/test_netsink_*")
end

--- ports are created from the lua_str `ports` table with the right types
function TestNetsink:Test_ports_created()
   local sink = launch_sink("ports = { x='double', n='int32_t' }")
   local px = ubx.block_port_get(sink, "x")
   local pn = ubx.block_port_get(sink, "n")
   lu.assert_not_nil(px)
   lu.assert_not_nil(pn)
   lu.assert_equals(ffi.string(px.in_type.name), "double")
   lu.assert_equals(ffi.string(pn.in_type.name), "int32_t")
end

--- JSON over UDP (default): a connected "ts" port overrides the auto ts
function TestNetsink:Test_json_ts_from_port()
   local sink, udp_port = launch_sink("ports = { ts='double', a='double' }")
   local rx = make_receiver(udp_port)

   local p_ts = ubx.port_clone_conn(sink, "ts", 1, nil, 7, 0)
   local p_a  = ubx.port_clone_conn(sink, "a",  1, nil, 7, 0)
   ubx.block_tostate(sink, 'active')

   p_ts:write(1.5)
   p_a:write(42.0)
   sink:do_step()

   local o = recv(rx, json.decode)
   ffi.C.close(rx)

   lu.assert_not_nil(o)
   lu.assert_almost_equals(o.ts, 1.5, 1e-9)
   lu.assert_almost_equals(o.a, 42.0, 1e-9)
end

--- one step drains ALL buffered samples in FIFO order (decoupled sink)
function TestNetsink:Test_json_drains_all_buffered()
   local sink, udp_port = launch_sink("ports = { ts='double', a='double' }")
   local rx = make_receiver(udp_port)

   local N = 8
   local p_ts = ubx.port_clone_conn(sink, "ts", 16, nil, 7, 0)
   local p_a  = ubx.port_clone_conn(sink, "a",  16, nil, 7, 0)
   ubx.block_tostate(sink, 'active')

   for i = 1, N do p_ts:write(i + 0.0); p_a:write(i * 10.0) end
   sink:do_step()

   for i = 1, N do
      local o = recv(rx, json.decode)
      lu.assert_not_nil(o, "missing datagram " .. i)
      lu.assert_almost_equals(o.ts, i + 0.0, 1e-9)
      lu.assert_almost_equals(o.a, i * 10.0, 1e-9)
   end
   lu.assert_nil(recv(rx, json.decode))
   ffi.C.close(rx)
end

--- format="msgpack" (lua_str global): binary frames decode back
function TestNetsink:Test_msgpack_format()
   if not has_mp then
      io.stderr:write("WARNING: test_netsink: skipping msgpack (no lua-MessagePack)\n")
      return
   end
   local sink, udp_port = launch_sink("ports = { ts='double', a='double' }",
				      "format='msgpack'\n")
   local rx = make_receiver(udp_port)

   local p_ts = ubx.port_clone_conn(sink, "ts", 1, nil, 7, 0)
   local p_a  = ubx.port_clone_conn(sink, "a",  1, nil, 7, 0)
   ubx.block_tostate(sink, 'active')

   p_ts:write(2.5)
   p_a:write(21.0)
   sink:do_step()

   local o = recv(rx, mp.unpack)
   ffi.C.close(rx)

   lu.assert_not_nil(o)
   lu.assert_almost_equals(o.ts, 2.5, 1e-9)
   lu.assert_almost_equals(o.a, 21.0, 1e-9)
end

--- a "type[N]" port is created as an array inport of length N
function TestNetsink:Test_array_port_created()
   local sink = launch_sink("ports = { pos='double[3]', a='double' }")
   local ppos = ubx.block_port_get(sink, "pos")
   local pa   = ubx.block_port_get(sink, "a")
   lu.assert_not_nil(ppos)
   lu.assert_equals(ffi.string(ppos.in_type.name), "double")
   lu.assert_equals(tonumber(ppos.in_data_len), 3)
   lu.assert_equals(tonumber(pa.in_data_len), 1)   -- bare type stays scalar
end

--- an array port serializes as a nested JSON array under its key
function TestNetsink:Test_json_array_port()
   local sink, udp_port = launch_sink("ports = { ts='double', pos='double[3]' }")
   local rx = make_receiver(udp_port)

   local p_ts  = ubx.port_clone_conn(sink, "ts",  1, nil, 7, 0)
   local p_pos = ubx.port_clone_conn(sink, "pos", 1, nil, 7, 0)
   ubx.block_tostate(sink, 'active')

   p_ts:write(1.0)
   p_pos:write({ 1.5, -2.5, 3.5 })
   sink:do_step()

   local o = recv(rx, json.decode)
   ffi.C.close(rx)

   lu.assert_not_nil(o)
   lu.assert_equals(o.pos, { 1.5, -2.5, 3.5 })
end

--- a struct port serializes as a nested JSON object
function TestNetsink:Test_json_struct_port()
   local sink, udp_port = launch_sink("ports = { ts='double', per='struct ptrig_period' }")
   local rx = make_receiver(udp_port)

   local p_ts  = ubx.port_clone_conn(sink, "ts",  1, nil, 7, 0)
   local p_per = ubx.port_clone_conn(sink, "per", 1, nil, 7, 0)
   ubx.block_tostate(sink, 'active')

   p_ts:write(1.0)
   p_per:write({ sec = 2, usec = 500 })
   sink:do_step()

   local o = recv(rx, json.decode)
   ffi.C.close(rx)

   lu.assert_not_nil(o)
   lu.assert_equals(o.per, { sec = 2, usec = 500 })
end

--- a zero-length array suffix is rejected at init
function TestNetsink:Test_bad_array_len_fails()
   lu.assert_false(pcall(launch_sink, "ports = { p='double[0]' }"))
end

--- an unknown transport makes start refuse (nonzero, block not activated)
function TestNetsink:Test_bad_transport_fails()
   local sink = launch_sink("ports = { a='double' }", "transport='carrier-pigeon'\n")
   ubx.port_clone_conn(sink, "a", 1, nil, 7, 0)
   lu.assert_not_equals(ubx.block_tostate(sink, 'active'), 0)
end

--- an unknown type in `ports` fails block init
function TestNetsink:Test_bad_type_fails()
   lu.assert_false(pcall(launch_sink, "ports = { a='no_such_type' }"))
end


-- zmq subscriber
ffi.cdef[[
void *zmq_ctx_new(void);
int zmq_ctx_term(void *c);
void *zmq_socket(void *c, int type);
int zmq_close(void *s);
int zmq_connect(void *s, const char *addr);
int zmq_setsockopt(void *s, int option, const void *val, size_t len);
int zmq_getsockopt(void *s, int option, void *val, size_t *len);
int zmq_recv(void *s, void *buf, size_t len, int flags);
]]

local has_zmq, zmq = pcall(ffi.load, "zmq")
local ZMQ_SUB, ZMQ_SUBSCRIBE, ZMQ_RCVTIMEO, ZMQ_RCVMORE = 2, 6, 27, 13
local next_ipc = 0

local function ipc_uri()
   next_ipc = next_ipc + 1
   return ("ipc:///tmp/test_netsink_%d_%d"):format(ffi.C.getpid(), next_ipc)
end

local function sub_open(uri, prefix)
   local ctx = zmq.zmq_ctx_new()
   local s = zmq.zmq_socket(ctx, ZMQ_SUB)
   local tmo = ffi.new("int[1]", 20)
   zmq.zmq_setsockopt(s, ZMQ_RCVTIMEO, tmo, ffi.sizeof(tmo))
   zmq.zmq_setsockopt(s, ZMQ_SUBSCRIBE, prefix or "", #(prefix or ""))
   assert(zmq.zmq_connect(s, uri) == 0)
   return { ctx=ctx, s=s }
end

local function sub_close(sub)
   zmq.zmq_close(sub.s)
   zmq.zmq_ctx_term(sub.ctx)
end

-- all frames of one message, nil on timeout
local function sub_recv(sub)
   local buf = ffi.new("char[?]", 4096)
   local more, msize = ffi.new("int[1]"), ffi.new("size_t[1]", ffi.sizeof("int"))
   local frames = {}
   repeat
      local n = zmq.zmq_recv(sub.s, buf, 4096, 0)
      if n < 0 then return nil end
      frames[#frames+1] = ffi.string(buf, n)
      zmq.zmq_getsockopt(sub.s, ZMQ_RCVMORE, more, msize)
   until more[0] == 0
   return frames
end

-- write and step until a message arrives
local function exchange(sink, sub, write)
   for _ = 1, 200 do
      write()
      sink:do_step()
      local m = sub_recv(sub)
      if m then return m end
   end
end

local function hashstr(nd, tname)
   local buf = ffi.new("char[33]")
   ubx.type_hashstr(ubx.type_get(nd, tname), buf)
   return ffi.string(buf)
end

local function start_fails(sink)
   return ubx.block_tostate(sink, 'active') ~= 0
end

--- json with the default topic reads like in PlotJuggler's ZMQ
--- subscriber: first frame topic, second frame payload
function TestNetsink:Test_zmq_json_topic()
   if not has_zmq then return end
   local uri = ipc_uri()
   local sink = launch_sink("ports = { ts='double', a='double' }",
			    ("transport='zmq'\nuri='%s'\n"):format(uri))
   local p_ts = ubx.port_clone_conn(sink, "ts", 1, nil, 7, 0)
   local p_a  = ubx.port_clone_conn(sink, "a",  1, nil, 7, 0)
   ubx.block_tostate(sink, 'active')
   local sub = sub_open(uri)

   local m = exchange(sink, sub, function() p_ts:write(1.5); p_a:write(42.0) end)
   sub_close(sub)
   lu.assert_not_nil(m)
   lu.assert_equals(#m, 2)
   lu.assert_equals(m[1], "sink")
   lu.assert_almost_equals(json.decode(m[2]).a, 42.0, 1e-9)
end

--- subscribers filter on the configured topic
function TestNetsink:Test_zmq_topic_filter()
   if not has_zmq then return end
   local uri = ipc_uri()
   local sink = launch_sink("ports = { ts='double' }",
			    ("transport='zmq'\nuri='%s'\ntopic='arm'\n"):format(uri))
   local p_ts = ubx.port_clone_conn(sink, "ts", 1, nil, 7, 0)
   ubx.block_tostate(sink, 'active')
   local other, sub = sub_open(uri, "cam"), sub_open(uri, "arm")

   local m = exchange(sink, sub, function() p_ts:write(1.0) end)
   lu.assert_not_nil(m)
   lu.assert_equals(m[1], "arm")
   lu.assert_nil(sub_recv(other))
   sub_close(other)
   sub_close(sub)
end

--- raw: one message per port, type frame and the port bytes
function TestNetsink:Test_zmq_raw_array()
   if not has_zmq then return end
   local uri = ipc_uri()
   local sink = launch_sink("ports = { pos='double[3]' }",
			    ("transport='zmq'\nformat='raw'\nuri='%s'\n"):format(uri))
   local p = ubx.port_clone_conn(sink, "pos", 1, nil, 7, 0)
   ubx.block_tostate(sink, 'active')
   local sub = sub_open(uri)

   local m = exchange(sink, sub, function() p:write({ 1.0, 2.0, 3.0 }) end)
   sub_close(sub)
   lu.assert_not_nil(m)
   lu.assert_equals(#m, 3)
   lu.assert_equals(m[1], "sink/pos")
   lu.assert_equals(m[2], hashstr(ni, "double") .. " 3")
   lu.assert_equals(#m[3], 24)
   local d = ffi.cast("const double *", m[3])
   lu.assert_equals({ d[0], d[1], d[2] }, { 1.0, 2.0, 3.0 })
end

--- a handler makes the buffers of a by-reference type explicit
function TestNetsink:Test_zmq_raw_handler()
   if not has_zmq then return end
   local uri = ipc_uri()
   local sink = launch_sink("ports = { r='struct ubx_tstat' }",
      ("transport='zmq'\nformat='raw'\nuri='%s'\n"):format(uri) ..
      ("package.path=%q .. package.path\n"):format(here .. "?.lua;") ..
      "handlers = { ['struct ubx_tstat'] = 'netsink_test_handler' }\n")
   local p = ubx.port_clone_conn(sink, "r", 1, nil, 7, 0)
   ubx.block_tostate(sink, 'active')
   local sub = sub_open(uri)

   local m = exchange(sink, sub, function() p:write({ cnt=5 }) end)
   sub_close(sub)
   lu.assert_not_nil(m)
   lu.assert_equals(#m, 5)
   lu.assert_equals(m[1], "sink/r")
   lu.assert_equals(m[2], hashstr(ni, "struct ubx_tstat") .. " 1")
   lu.assert_equals(m[4], hashstr(ni, "int32_t") .. " 2")
   local d = ffi.cast("const int32_t *", m[5])
   lu.assert_equals({ d[0], d[1] }, { 11, 22 })
end

function TestNetsink:Test_zmq_config_errors()
   if not has_zmq then return end
   local uri = ipc_uri()
   lu.assert_true(start_fails(launch_sink("ports = { a='double' }", "topic='x'\n")))
   ubx.node_rm(ni); ni = nil
   lu.assert_true(start_fails(launch_sink("ports = { a='double' }", "format='raw'\n")))
   ubx.node_rm(ni); ni = nil
   lu.assert_true(start_fails(launch_sink("ports = { a='double' }",
      ("transport='zmq'\nuri='%s'\nhandlers={ double='x' }\n"):format(uri))))
   ubx.node_rm(ni); ni = nil
   lu.assert_true(start_fails(launch_sink("ports = { a='double' }",
      ("transport='zmq'\nformat='raw'\nuri='%s'\nhandlers={ double='no_such_module' }\n"):format(uri))))
end

if not _RUNNER then
   os.exit(lu.LuaUnit.run())
end
