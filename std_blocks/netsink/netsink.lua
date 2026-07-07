--
-- netsink: stream input port values as datagrams/messages over the
-- network, for live plotting and telemetry.
--
-- Two axes are configurable:
--
--   * transport : "udp" (raw UDP datagrams) or "zmq" (a ZeroMQ PUB
--                 socket, one message per frame)
--   * format    : "json" (newline-delimited, PlotJuggler-friendly) or
--                 "msgpack" (compact binary, via lua-MessagePack)
--
-- This is an ordinary `ubx/luablock`: instantiate it as
-- `luablock:netsink`. All configuration -- the port set and the network
-- settings -- is passed as globals via the block's `lua_str` config:
--
--   { name="sink", type="luablock:netsink" },
--   { name="sink", config = {
--        lua_str = [[
--           ports     = { ts="double", sin="double", pos="double[3]" }
--           transport = "zmq"              -- "udp" (default) | "zmq"
--           format    = "msgpack"          -- "json" (default) | "msgpack"
--           uri       = "tcp://*:9870"     -- zmq endpoint
--           -- host/port are the UDP equivalents (transport="udp")
--        ]],
--   } },
--
-- One input port is created per `ports` entry (key = port name / output
-- key, value = registered ubx type name). A "[N]" suffix on the type
-- (e.g. "double[3]") makes an array port of length N; a bare type name
-- is a scalar. Array ports serialize as a nested array under their key
-- (PlotJuggler expands these into name/0, name/1, ...). The ports are
-- added in the `preinit` hook -- the life-cycle step meant for extending
-- the block interface -- so they exist by the time the USC connections
-- are wired up. The network settings (transport/format/host/port/uri)
-- are read in `start`.
--
-- No core extension is required: no custom block type, no struct type
-- registration -- just the stock luablock and its lua_str/lua_file
-- mechanism.
--

local ubx = require("ubx")
local ffi = require("ffi")

-- encoders (optional; only the one selected by `format` must be present)
local has_json, json = pcall(require, "cjson")
if not has_json then has_json, json = pcall(require, "json") end

local has_mp, mp = pcall(require, "MessagePack")   -- lua-MessagePack (pure Lua)
if not has_mp then has_mp, mp = pcall(require, "messagepack") end
if not has_mp then has_mp, mp = pcall(require, "cmsgpack") end

-- Raw UDP and ZeroMQ via FFI (no luasocket/lzmq dependency). Only the
-- socket calls use FFI; all of these have a fixed, stable C ABI. int
-- fds, size_t/ssize_t for send, a 16-byte sockaddr_in (identical on 32-
-- and 64-bit ARM and x86). Timestamps use ubx.gettime() instead of a raw
-- clock_gettime FFI, whose struct timespec layout is *not* portable
-- across 32-bit time_t configurations.
ffi.cdef[[
int socket(int domain, int type, int protocol);
int close(int fd);
long sendto(int fd, const void *buf, unsigned long len, int flags,
	    const void *dest_addr, unsigned int addrlen);
unsigned short htons(unsigned short hostshort);
struct netsink_in_addr { uint32_t s_addr; };
int inet_aton(const char *cp, struct netsink_in_addr *inp);
struct netsink_sockaddr_in {
	unsigned short sin_family;
	unsigned short sin_port;
	struct netsink_in_addr sin_addr;
	char sin_zero[8];
};

/* libzmq (loaded lazily via ffi.load only when transport == "zmq") */
void *zmq_ctx_new(void);
int   zmq_ctx_term(void *context);
void *zmq_socket(void *context, int type);
int   zmq_close(void *s);
int   zmq_bind(void *s, const char *addr);
int   zmq_connect(void *s, const char *addr);
int   zmq_send(void *s, const void *buf, size_t len, int flags);
]]

local AF_INET = 2
local SOCK_DGRAM = 2
local ZMQ_PUB = 1
local ZMQ_DONTWAIT = 1

-- per-instance state: each luablock instance has its own lua_State, so
-- these file-locals are private to the instance.
local transport = nil  -- "udp" | "zmq"
local send = nil       -- function(msg): fire-and-forget, never blocks
local encode = nil     -- function(sample) -> string

-- udp transport state
local sock = -1
local dest = nil
local destlen = 0

-- zmq transport state
local zmq = nil        -- ffi.load("zmq") handle
local zctx = nil
local zsock = nil

local portlist = {}  -- ordered { {id=, typ=}, ... }   built in preinit
local portobjs = {}  -- { {id=, port=, rbuf=, pending=, miss=}, ... } resolved in start
local has_ts_port = false  -- true if a port named "ts" supplies the timestamp

-- sink steps an incomplete frame is held for a lagging port before being
-- sent without it
local GRACE_STEPS = 1

-- portable monotonic-ish time as seconds (clock source chosen at build
-- time: CLOCK_MONOTONIC by default, TSC/CNTVCT otherwise).
local function mono_now()
   local ts = ubx.gettime()
   return tonumber(ts.sec) + tonumber(ts.nsec) * 1e-9
end

-- Read a network setting from a lua_str global, else the default.
local function opt_str(name, default)
   if type(_G[name]) == "string" and #_G[name] > 0 then return _G[name] end
   return default
end

local function opt_num(name, default)
   if type(_G[name]) == "number" and _G[name] > 0 then return _G[name] end
   return default
end

--                          per-instance hooks
--
-- All configuration arrives as globals set by lua_str. `ports` must be
-- available here, in preinit, to build the interface; the network
-- settings are read later, in start.

function preinit(b)
   b = ffi.cast("ubx_block_t*", b)
   local nd = b.nd

   if type(ports) ~= "table" then
      ubx.err(nd, "netsink", "config 'ports' missing or not a table (set it via lua_str)")
      return false
   end

   for id, typ in pairs(ports) do
      if type(id) ~= "string" or id == "" then
	 ubx.err(nd, "netsink", "invalid port id '%s'", tostring(id))
	 return false
      end
      if type(typ) ~= "string" or typ == "" then
	 ubx.err(nd, "netsink", "port '%s': value must be a type-name string", id)
	 return false
      end
      -- optional "[N]" suffix selects an array port of length N; a bare
      -- type name is a scalar (length 1). Vectors serialize as arrays.
      local base, n = string.match(typ, "^%s*(.-)%s*%[%s*(%d+)%s*%]%s*$")
      local tname = base or typ
      local len = base and tonumber(n) or 1
      if len < 1 then
	 ubx.err(nd, "netsink", "port '%s': array length must be >= 1", id)
	 return false
      end
      if ubx.type_get(nd, tname) == nil then
	 ubx.err(nd, "netsink", "port '%s': unknown type '%s'", id, tname)
	 return false
      end
      if b:inport_add(id, "netsink input", 0, tname, len) ~= 0 then
	 ubx.err(nd, "netsink", "failed to add inport '%s'", id)
	 return false
      end
      portlist[#portlist+1] = { id=id, typ=tname, len=len }
      if id == "ts" then has_ts_port = true end
   end

   if #portlist == 0 then
      ubx.err(nd, "netsink", "config 'ports' is empty")
      return false
   end
   return true
end

-- transport setup: each returns true on success and installs the module
-- `send` closure. Failures log via ubx.err and return false.

local function setup_udp(nd)
   local h = opt_str("host", "127.0.0.1")
   local p = opt_num("port", 9870)

   sock = ffi.C.socket(AF_INET, SOCK_DGRAM, 0)
   if sock < 0 then
      ubx.err(nd, "netsink", "socket() failed")
      return false
   end

   dest = ffi.new("struct netsink_sockaddr_in")
   dest.sin_family = AF_INET
   dest.sin_port = ffi.C.htons(p)
   if ffi.C.inet_aton(h, dest.sin_addr) == 0 then
      ffi.C.close(sock); sock = -1
      ubx.err(nd, "netsink", "invalid host '%s'", tostring(h))
      return false
   end
   destlen = ffi.sizeof(dest)

   send = function(msg)
      ffi.C.sendto(sock, msg, #msg, 0, ffi.cast("void*", dest), destlen)
   end

   ubx.info(nd, "netsink", "udp -> %s:%d", h, p)
   return true
end

local function setup_zmq(nd)
   local uri = opt_str("uri", "tcp://*:9870")
   local do_bind = opt_num("zmq_bind", 1) ~= 0   -- default: bind

   local ok, lib = pcall(ffi.load, "zmq")
   if not ok then
      ubx.err(nd, "netsink", "transport 'zmq': failed to load libzmq (%s)", tostring(lib))
      return false
   end
   zmq = lib

   zctx = zmq.zmq_ctx_new()
   if zctx == nil then
      ubx.err(nd, "netsink", "zmq_ctx_new() failed")
      return false
   end
   zsock = zmq.zmq_socket(zctx, ZMQ_PUB)
   if zsock == nil then
      zmq.zmq_ctx_term(zctx); zctx = nil
      ubx.err(nd, "netsink", "zmq_socket() failed")
      return false
   end

   local rc = do_bind and zmq.zmq_bind(zsock, uri) or zmq.zmq_connect(zsock, uri)
   if rc ~= 0 then
      zmq.zmq_close(zsock); zsock = nil
      zmq.zmq_ctx_term(zctx); zctx = nil
      ubx.err(nd, "netsink", "zmq %s '%s' failed", do_bind and "bind" or "connect", uri)
      return false
   end

   -- DONTWAIT: a PUB with no (or a slow) subscriber must never block the
   -- decoupled sink thread; ZMQ drops such messages, which is fine here.
   send = function(msg)
      zmq.zmq_send(zsock, msg, #msg, ZMQ_DONTWAIT)
   end

   ubx.info(nd, "netsink", "zmq PUB %s %s", do_bind and "bind" or "connect", uri)
   return true
end

function start(b)
   b = ffi.cast("ubx_block_t*", b)
   local nd = b.nd

   -- select encoder. JSON is newline-delimited (each datagram/message is
   -- one object, PlotJuggler-friendly); msgpack frames are self-delimiting
   -- so no separator is added.
   local fmt = opt_str("format", "json")
   if fmt == "json" then
      if not has_json then
	 ubx.err(nd, "netsink", "format 'json' needs a json module (cjson or json)")
	 return false
      end
      encode = function(sample) return json.encode(sample).."\n" end
   elseif fmt == "msgpack" then
      if not has_mp then
	 ubx.err(nd, "netsink", "format 'msgpack' needs lua-MessagePack (or cmsgpack)")
	 return false
      end
      encode = function(sample) return mp.pack(sample) end
   else
      ubx.err(nd, "netsink", "unknown format '%s' (use 'json' or 'msgpack')", tostring(fmt))
      return false
   end

   -- select and open transport
   transport = opt_str("transport", "udp")
   local ok
   if transport == "udp" then ok = setup_udp(nd)
   elseif transport == "zmq" then ok = setup_zmq(nd)
   else
      ubx.err(nd, "netsink", "unknown transport '%s' (use 'udp' or 'zmq')", tostring(transport))
      return false
   end
   if not ok then return false end

   -- resolve port pointers and pre-allocate one read buffer per port
   for _, e in ipairs(portlist) do
      local pt = ubx.block_port_get(b, e.id)
      portobjs[#portobjs+1] = { id=e.id, port=pt, rbuf=ubx.port_alloc_read_sample(pt),
				pending=nil, miss=0 }
   end

   ubx.info(nd, "netsink", "streaming %s/%s (%d ports)", transport, fmt, #portobjs)
   return true
end

-- Drain all buffered samples, emitting one datagram/message per frame
-- (one sample per port), so a sink slower than (and decoupled from) the
-- producers forwards every sample, not just the latest. Nothing is sent
-- if no port had data.
--
-- Frame alignment: the per-port buffers are filled non-atomically (the
-- producers write one port after another), so a decoupled sink can catch
-- a cycle half-written; reading each port independently would emit a
-- partial frame and skew that port permanently. Instead each sample is
-- parked in a per-port "pending" slot and a frame emitted only once every
-- port has one. An incomplete frame is held GRACE_STEPS steps for the
-- lagging port, then sent without it -- so a stopped input is dropped
-- gracefully (rejoining when it resumes) rather than stalling the stream.
--
-- "ts" comes from a "ts" input port if present, else ubx.gettime() at
-- read time. When decoupled the auto "ts" is the sink's read time (same
-- for every frame in a step), so feed a "ts" port for a real per-sample
-- axis (see netsink.usc).
function step(b)
   if send == nil then return end

   while true do
      -- top up pendings; present resets miss, recently-active absent waits
      local any_present = false
      local wait = false
      for _, p in ipairs(portobjs) do
	 if p.pending == nil and ubx.port_read(p.port, p.rbuf) > 0 then
	    p.pending = ubx.data_tolua(p.rbuf)
	 end
	 if p.pending ~= nil then
	    p.miss = 0
	    any_present = true
	 elseif p.miss < GRACE_STEPS then
	    wait = true
	 end
      end

      if not any_present then break end -- nothing buffered at all

      if wait then
	 -- hold one step for the lagging port(s); bump miss so a stopped
	 -- input is eventually dropped rather than waited on forever.
	 for _, p in ipairs(portobjs) do
	    if p.pending == nil then p.miss = p.miss + 1 end
	 end
	 break
      end

      -- frame complete (or lagging ports out of grace and omitted): emit
      local sample = {}
      for _, p in ipairs(portobjs) do
	 if p.pending ~= nil then
	    sample[p.id] = p.pending
	    p.pending = nil
	 end
      end
      if not has_ts_port then sample.ts = mono_now() end
      send(encode(sample))
   end
end

function stop(b)
   if sock >= 0 then ffi.C.close(sock); sock = -1 end
   if zsock ~= nil then zmq.zmq_close(zsock); zsock = nil end
   if zctx ~= nil then zmq.zmq_ctx_term(zctx); zctx = nil end
   send = nil
   portobjs = {}
   dest = nil
end

function cleanup(b)
   portlist = {}
   has_ts_port = false
   transport = nil
   encode = nil
end
