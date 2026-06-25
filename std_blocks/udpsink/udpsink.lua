--
-- udpsink: stream input port values as JSON datagrams over UDP.
--
-- Intended for live plotting (e.g. PlotJuggler's UDP JSON source).
-- This is an ordinary `ubx/luablock`: instantiate it as
-- `luablock:udpsink` and pass the port set + destination via the
-- block's `lua_str` config, e.g.
--
--   lua_str = [[
--      ports = { ramp="double", sin="double", cos="double" }
--      host  = "127.0.0.1"
--      port  = 9870
--   ]]
--
-- One input port is created per `ports` entry (key = port name / JSON
-- key, value = registered ubx type name). The ports are added in the
-- `preinit` hook -- the life-cycle step meant for extending the block
-- interface -- so they exist by the time the USC connections are wired
-- up.
--
-- No core extension is required: no custom block type, no struct type
-- registration -- just the stock luablock and its lua_str/lua_file
-- mechanism.
--

local ubx = require("ubx")
local ffi = require("ffi")

local has_json, json = pcall(require, "cjson")
if not has_json then has_json, json = pcall(require, "json") end
if not has_json then error("udpsink: no json module (cjson or json) found") end

-- Raw UDP via FFI (no luasocket dependency). Only the socket calls use
-- FFI; all of these have a fixed Linux ABI that is identical on 32- and
-- 64-bit ARM (and x86): int fds, size_t/ssize_t for sendto, a 16-byte
-- sockaddr_in. Timestamps use ubx.gettime() instead of a raw
-- clock_gettime FFI, whose struct timespec layout is *not* portable
-- across 32-bit time_t configurations.
ffi.cdef[[
int socket(int domain, int type, int protocol);
int close(int fd);
long sendto(int fd, const void *buf, unsigned long len, int flags,
	    const void *dest_addr, unsigned int addrlen);
unsigned short htons(unsigned short hostshort);
struct udpsink_in_addr { uint32_t s_addr; };
int inet_aton(const char *cp, struct udpsink_in_addr *inp);
struct udpsink_sockaddr_in {
	unsigned short sin_family;
	unsigned short sin_port;
	struct udpsink_in_addr sin_addr;
	char sin_zero[8];
};
]]

local AF_INET = 2
local SOCK_DGRAM = 2

-- per-instance state: each luablock instance has its own lua_State, so
-- these file-locals are private to the instance.
local sock = -1
local dest = nil
local destlen = 0
local portlist = {}  -- ordered { {id=, typ=}, ... }   built in init
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

--                          per-instance hooks
--
-- Config (`ports`, `host`, `port`) arrives as globals set by lua_str.

function preinit(b)
   b = ffi.cast("ubx_block_t*", b)
   local nd = b.nd

   if type(ports) ~= "table" then
      ubx.err(nd, "udpsink", "config 'ports' missing or not a table (set it via lua_str)")
      return false
   end

   for id, typ in pairs(ports) do
      if type(id) ~= "string" or id == "" then
	 ubx.err(nd, "udpsink", "invalid port id '%s'", tostring(id))
	 return false
      end
      if type(typ) ~= "string" or typ == "" then
	 ubx.err(nd, "udpsink", "port '%s': value must be a type-name string", id)
	 return false
      end
      if ubx.type_get(nd, typ) == nil then
	 ubx.err(nd, "udpsink", "port '%s': unknown type '%s'", id, typ)
	 return false
      end
      if b:inport_add(id, "udpsink input", 0, typ, 1) ~= 0 then
	 ubx.err(nd, "udpsink", "failed to add inport '%s'", id)
	 return false
      end
      portlist[#portlist+1] = { id=id, typ=typ }
      if id == "ts" then has_ts_port = true end
   end

   if #portlist == 0 then
      ubx.err(nd, "udpsink", "config 'ports' is empty")
      return false
   end
   return true
end

function start(b)
   b = ffi.cast("ubx_block_t*", b)
   local nd = b.nd

   local h = (type(host) == "string" and #host > 0) and host or "127.0.0.1"
   local p = (type(port) == "number" and port > 0) and port or 9870

   sock = ffi.C.socket(AF_INET, SOCK_DGRAM, 0)
   if sock < 0 then
      ubx.err(nd, "udpsink", "socket() failed")
      return false
   end

   dest = ffi.new("struct udpsink_sockaddr_in")
   dest.sin_family = AF_INET
   dest.sin_port = ffi.C.htons(p)
   if ffi.C.inet_aton(h, dest.sin_addr) == 0 then
      ffi.C.close(sock); sock = -1
      ubx.err(nd, "udpsink", "invalid host '%s'", tostring(h))
      return false
   end
   destlen = ffi.sizeof(dest)

   -- resolve port pointers and pre-allocate one read buffer per port
   for _, e in ipairs(portlist) do
      local pt = ubx.block_port_get(b, e.id)
      portobjs[#portobjs+1] = { id=e.id, port=pt, rbuf=ubx.port_alloc_read_sample(pt),
				pending=nil, miss=0 }
   end

   ubx.info(nd, "udpsink", "streaming JSON to %s:%d (%d ports)", h, p, #portobjs)
   return true
end

-- Drain all buffered samples, emitting one JSON datagram per frame (one
-- sample per port), so a sink slower than (and decoupled from) the
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
-- axis (see udpsink.usc).
function step(b)
   if sock < 0 then return end

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
      local msg = json.encode(sample).."\n"
      ffi.C.sendto(sock, msg, #msg, 0, ffi.cast("void*", dest), destlen)
   end
end

function stop(b)
   if sock >= 0 then ffi.C.close(sock); sock = -1 end
   portobjs = {}
   dest = nil
end

function cleanup(b)
   portlist = {}
   has_ts_port = false
end
