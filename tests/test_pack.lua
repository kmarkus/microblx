--
-- Test ubx/pack and ubx/unpack
--

local lu = require("luaunit")
local ubx = require("ubx")
local bd = require("blockdiagram")
local ffi = require("ffi")
local utils = require("utils")

local assert_equals = lu.assert_equals

local nd

TestPack = {}

function TestPack:setup()
   ubx.reset_block_uid()
end

function TestPack:teardown()
   if nd then ubx.node_rm(nd) end
   nd = nil
end

local PORTS = {
   { id = "u", type = "uint64_t", len = 1 },
   { id = "d", type = "double" },			-- len 0: 1
   { id = "ts", type = "struct ubx_tstat", len = 1 },
   { id = "arr", type = "int32_t", len = 4 },
}

local function node(name)
   nd = ubx.node_create(name, { loglevel = ffi.C.UBX_LOGLEVEL_CRIT })
   for _, m in ipairs { "stdtypes", "lfrb", "vstore", "pack" } do
      ubx.load_module(nd, m)
   end
   ubx.ffi_load_types(nd)
end

-- create and init a pack block, with a write port per input
local function mkpack(ports, buffer_len)
   local pk = ubx.block_create(nd, "ubx/pack", "pk")
   ubx.do_configure(pk, { packid = "pk", ports = ports, buffer_len = buffer_len })
   local w = {}
   for _, p in ipairs(ports) do w[p.id] = ubx.port_clone_conn(pk, p.id) end
   assert_equals(ubx.block_tostate(pk, 'active'), 0)
   return pk, w
end

-- create and init an unpack block of pk, with a read port per output
local function mkunpack(ports, name, cfg)
   local up = ubx.block_create(nd, "ubx/unpack", name or "up")
   ubx.do_configure(up, cfg or { packid = "pk" })
   local r = { overruns = ubx.port_clone_conn(up, "overruns", 8) }
   for _, p in ipairs(ports) do r[p.id] = ubx.port_clone_conn(up, p.id, 8) end
   assert_equals(ubx.block_tostate(up, 'active'), 0)
   return up, r
end

-- read a port: value or nil if no data
local function rd(p)
   local len, val = p:read()
   if len <= 0 then return nil end
   return val:tolua()
end

local function num(p) local v = rd(p) return v and tonumber(v) end

--- scalars, a struct and an array survive the round trip
function TestPack:TestRoundTrip()
   node("TestPackRoundTrip")
   local pk, w = mkpack(PORTS)
   local up, r = mkunpack(PORTS)

   w.u:write(0x1122334455667788ULL)
   w.d:write(3.25)
   local ts = ubx.data_alloc(nd, "struct ubx_tstat", 1)
   ubx.data_set(ts, { id = "chain0", min = { sec = 1, nsec = 2 }, cnt = 678 })
   w.ts:write(ts)
   w.arr:write({ -1, 2, -3, 4 })

   pk:do_step()
   up:do_step()

   local len, uval = r.u:read()
   assert_equals(tonumber(len), 1)
   assert_equals(ffi.cast("uint64_t*", uval.data)[0], 0x1122334455667788ULL)
   assert_equals(num(r.d), 3.25)
   local v = rd(r.ts)
   assert_equals(v.id, "chain0")
   assert_equals(tonumber(v.min.sec), 1)
   assert_equals(tonumber(v.min.nsec), 2)
   assert_equals(tonumber(v.cnt), 678)
   local a = rd(r.arr)
   assert_equals({ a[1], a[2], a[3], a[4] }, { -1, 2, -3, 4 })
   assert_equals(rd(r.overruns), nil)

   -- no new frame: nothing written
   up:do_step()
   assert_equals(rd(r.u), nil)
end

--- a port without data in a step is not written by unpack
function TestPack:TestPresent()
   node("TestPackPresent")
   local ports = { { id = "a", type = "uint32_t" }, { id = "b", type = "double" } }
   local pk, w = mkpack(ports)
   local up = ubx.block_create(nd, "ubx/unpack", "up")
   ubx.do_configure(up, { packid = "pk" })
   local ra = ubx.port_clone_conn(up, "a", 8)
   local vs = ubx.block_create(nd, "ubx/vstore", "vs", { type_name = "double" })
   assert_equals(ubx.block_tostate(vs, 'active'), 0)
   assert_equals(ubx.ubx.ubx_port_connect_out(ubx.port_get(up, "b"), vs), 0)
   assert_equals(ubx.block_tostate(up, 'active'), 0)
   local dat = ubx.data_alloc(nd, "double", 1)

   w.a:write(7)
   pk:do_step()
   up:do_step()
   assert_equals(num(ra), 7)
   assert_equals(tonumber(ubx.interaction_read(vs, dat)), 0)

   w.b:write(1.5)
   pk:do_step()
   up:do_step()
   assert_equals(rd(ra), nil)
   assert_equals(tonumber(ubx.interaction_read(vs, dat)), 1)
   assert_equals(tonumber(dat:tolua()), 1.5)
end

local SEQ = { { id = "seq", type = "uint64_t" } }

-- write v into pk and step it
local function produce(pk, w, v)
   w.seq:write(v)
   pk:do_step()
end

--- buffer_len 1: an on-time reader gets every frame, one 2 behind gets
--- one overrun and then the newest frame
function TestPack:TestBufferLen1()
   node("TestPackBuf1")
   local pk, w = mkpack(SEQ, 1)
   local up, r = mkunpack(SEQ)

   for v = 1, 5 do
      produce(pk, w, v)
      up:do_step()
      assert_equals(num(r.seq), v)
   end

   produce(pk, w, 6)
   produce(pk, w, 7)
   up:do_step()
   assert_equals(num(r.overruns), 1)
   assert_equals(num(r.seq), 7)
   up:do_step()
   assert_equals(num(r.seq), nil)
   assert_equals(num(r.overruns), nil)
end

--- buffer_len 3: a reader 3 behind gets all frames in order, one 5
--- behind one overrun and then the last 3
function TestPack:TestBufferLen3()
   node("TestPackBuf3")
   local pk, w = mkpack(SEQ, 3)
   local up, r = mkunpack(SEQ)

   for v = 1, 3 do produce(pk, w, v) end
   for v = 1, 3 do
      up:do_step()
      assert_equals(num(r.seq), v)
   end
   up:do_step()
   assert_equals(num(r.seq), nil)
   assert_equals(num(r.overruns), nil)

   for v = 4, 8 do produce(pk, w, v) end
   for v = 6, 8 do
      up:do_step()
      assert_equals(num(r.seq), v)
   end
   assert_equals(num(r.overruns), 1)
end

--- two readers are independent
function TestPack:TestTwoReaders()
   node("TestPackTwoReaders")
   local pk, w = mkpack(SEQ, 1)
   local up1, r1 = mkunpack(SEQ, "up1")
   local up2, r2 = mkunpack(SEQ, "up2")

   produce(pk, w, 1)
   up1:do_step()
   produce(pk, w, 2)
   up1:do_step()
   up2:do_step()
   assert_equals(num(r1.seq), 1)
   assert_equals(num(r1.seq), 2)
   assert_equals(num(r1.overruns), nil)
   assert_equals(num(r2.overruns), 1)
   assert_equals(num(r2.seq), 2)
end

local function init_fails(type, cfg)
   local b = ubx.block_create(nd, type, "bad")
   local ok = pcall(ubx.do_configure, b, cfg)
   lu.assert_false(ok)
   assert_equals(b:get_block_state(), 'preinit')
end

--- bad configs, an unknown type, a missing segment and a mismatching
--- expected layout fail init
function TestPack:TestInitErrors()
   node("TestPackInitErrors")
   init_fails("ubx/pack", { packid = "bad", ports = { { id = "x", type = "nosuchtype" } } })
   ubx.block_rm(nd, "bad")
   init_fails("ubx/pack", { packid = "bad", ports = { { id = "x", type = "double" },
				      { id = "x", type = "double" } } })
   ubx.block_rm(nd, "bad")
   init_fails("ubx/pack", { packid = "bad", ports = { { id = "overruns", type = "double" } } })
   ubx.block_rm(nd, "bad")
   init_fails("ubx/unpack", { packid = "pk" })	-- no segment yet
   ubx.block_rm(nd, "bad")

   mkpack(PORTS)
   local exp = { { id = "u", type = "uint64_t" }, { id = "d", type = "float" },
		 { id = "ts", type = "struct ubx_tstat" }, { id = "arr", type = "int32_t", len = 4 } }
   init_fails("ubx/unpack", { packid = "pk", ports = exp })
   ubx.block_rm(nd, "bad")
   init_fails("ubx/unpack", { packid = "pk", ports = { PORTS[1] } })
   ubx.block_rm(nd, "bad")

   exp[2].type = "double"
   local up = ubx.block_create(nd, "ubx/unpack", "good")
   ubx.do_configure(up, { packid = "pk", ports = exp })
   assert_equals(up:get_block_state(), 'inactive')
end

--- packid is relative to the subsystem in the block name, or absolute
function TestPack:TestScope()
   node("TestPackScope")
   local ports = { { id = "u", type = "uint64_t" } }
   local function mk(btype, name, packid, cfg)
      local b = ubx.block_create(nd, btype, name)
      cfg = cfg or {}
      cfg.packid = packid
      if btype == "ubx/pack" then cfg.ports = ports end
      ubx.do_configure(b, cfg)
      assert_equals(b:get_block_state(), 'inactive')
      return b
   end
   local function exists(id)
      local f = io.open("/dev/shm/"..id)
      if f then f:close() end
      return f ~= nil
   end

   mk("ubx/pack", "top", "pk")
   mk("ubx/pack", "a/pk", "pk")
   mk("ubx/pack", "b/pk", "pk")
   mk("ubx/pack", "a/c/pk", "pk")
   mk("ubx/pack", "a/s", "c/x")
   mk("ubx/pack", "y", "/shared")

   for _, id in ipairs { "pk", "a.pk", "b.pk", "a.c.pk", "a.c.x", "shared" } do
      lu.assert_true(exists("TestPackScope."..id), id)
   end

   -- readers of them, from elsewhere
   mk("ubx/unpack", "up0", "a/pk")
   mk("ubx/unpack", "z/up1", "/a/c/pk")
   mk("ubx/unpack", "q/up2", "/shared")
   mk("ubx/unpack", "up3", "TestPackScope:/b/pk")
end

--- what packid rejects
function TestPack:TestBadPackids()
   node("TestPackBadIds")
   for _, id in ipairs { "", "/", "a/", "/a/", "a//b", "a.b", "/a.b", ".a", "a.",
			 ":/a", "x:", "x:a", "x:/", "x.y:/a" } do
      init_fails("ubx/pack", { packid = id, ports = { { id = "u", type = "double" } } })
      ubx.block_rm(nd, "bad")
   end
   local b = ubx.block_create(nd, "ubx/pack", "s.t/pk")
   lu.assert_false(pcall(ubx.do_configure, b, { packid = "pk",
						ports = { { id = "u", type = "double" } } }))
end

local U64 = { { id = "u", type = "uint64_t" } }

--- whoever comes first creates: an unpack with ports, before its pack
function TestPack:TestUnpackFirst()
   node("TestPackUpFirst")
   local up = ubx.block_create(nd, "ubx/unpack", "up")
   ubx.do_configure(up, { packid = "pk", ports = U64 })
   local r = ubx.port_clone_conn(up, "u", 8)
   local pk = ubx.block_create(nd, "ubx/pack", "w")
   ubx.do_configure(pk, { packid = "pk", ports = U64 })
   local w = ubx.port_clone_conn(pk, "u")
   assert_equals(ubx.block_tostate(pk, 'active'), 0)
   assert_equals(ubx.block_tostate(up, 'active'), 0)
   w:write(5)
   pk:do_step()
   up:do_step()
   assert_equals(num(r), 5)
end

--- a pinned segment must match, in ports and an explicit buffer_len
function TestPack:TestPinned()
   node("TestPackPinned")
   local up = ubx.block_create(nd, "ubx/unpack", "up")
   ubx.do_configure(up, { packid = "pk", ports = U64, buffer_len = 4 })

   init_fails("ubx/pack", { packid = "pk", ports = { { id = "u", type = "double" } } })
   ubx.block_rm(nd, "bad")
   init_fails("ubx/pack", { packid = "pk", ports = U64, buffer_len = 5 })
   ubx.block_rm(nd, "bad")
   init_fails("ubx/unpack", { packid = "pk", ports = { { id = "x", type = "uint64_t" } } })
   ubx.block_rm(nd, "bad")

   -- an unset one takes what is there
   local pk = ubx.block_create(nd, "ubx/pack", "w")
   ubx.do_configure(pk, { packid = "pk", ports = U64 })
   assert_equals(pk:get_block_state(), 'inactive')
end

--- one writer, also if it is a tool
function TestPack:TestOneWriter()
   node("TestPackOneWriter")
   mkpack(U64)
   init_fails("ubx/pack", { packid = "pk", ports = U64 })
   ubx.block_rm(nd, "bad")

   local r = os.execute("command -v ubx-pack >/dev/null 2>&1")
   lu.skipIf(not (r == 0 or r == true), "ubx-pack not found")

   local p = io.popen("printf -- '--ports={{id=\"u\",len=1,type=\"uint64_t\"}}\\n'"
		      .." | ubx-pack write TestPackOneWriter.pk 2>&1 >/dev/null; echo rc=$?")
   local out = p:read("*a")
   p:close()
   lu.assert_str_contains(out, "rc=1")
end

--- a leftover is replaced, the last one out removes the segment
function TestPack:TestLifetime()
   local id = "/dev/shm/TestPackLeft.pk"
   local script = os.tmpname()
   local f = io.open(script, "w")
   f:write([[
local ubx = require("ubx")
local nd = ubx.node_create("TestPackLeft", { loglevel = 2 })
for _, m in ipairs { "stdtypes", "lfrb", "pack" } do ubx.load_module(nd, m) end
local pk = ubx.block_create(nd, "ubx/pack", "pk")
ubx.do_configure(pk, { packid = "pk", ports = { { id = "u", type = "double" } } })
os.exit(0)
]])
   f:close()
   os.execute("luajit "..script.." 2>/dev/null")
   os.remove(script)
   f = io.open(id)
   lu.assert_not_nil(f)
   f:close()

   node("TestPackLeft")
   local pk = mkpack(U64)		-- another layout
   local up = ubx.block_create(nd, "ubx/unpack", "up")
   ubx.do_configure(up, { packid = "pk", ports = U64 })

   -- the writer goes, the pin of up keeps it
   assert_equals(ubx.block_tostate(pk, 'preinit'), 0)
   f = io.open(id)
   lu.assert_not_nil(f)
   f:close()

   assert_equals(ubx.block_tostate(up, 'preinit'), 0)
   lu.assert_nil(io.open(id))
end

--- never touch what is no pack segment
function TestPack:TestForeign()
   local id = "/dev/shm/TestPackForeign.x"
   local f = io.open(id, "w")
   f:write(string.rep("x", 4096))
   f:close()

   node("TestPackForeign")
   init_fails("ubx/pack", { packid = "/x", ports = U64 })
   local g = io.open(id)
   lu.assert_not_nil(g)
   g:close()
   os.remove(id)
end

--- the pack block of a node in another process is reached with the
--- node config
local UNPACK_CHILD = [[
local ubx = require("ubx")
local ffi = require("ffi")
ffi.cdef("int usleep(unsigned);")
local nd = ubx.node_create("TestPackNodeB", { loglevel = 2 })
for _, m in ipairs { "stdtypes", "lfrb", "pack" } do ubx.load_module(nd, m) end
ubx.ffi_load_types(nd)
local up = ubx.block_create(nd, "ubx/unpack", "up")
ubx.do_configure(up, { packid = "TestPackNodeA:/pk" })
local r = ubx.port_clone_conn(up, "u", 8)
ubx.block_tostate(up, "active")
print("ready")
io.stdout:flush()
ffi.C.usleep(200000)
ubx.cblock_step(up)
local len, val = r:read()
print(len > 0 and tonumber(val:tolua()) or "none")
ubx.node_rm(nd)
]]

function TestPack:TestOtherNode()
   node("TestPackNodeA")
   local pk, w = mkpack({ { id = "u", type = "uint64_t" } })

   local script = os.tmpname()
   local f = io.open(script, "w")
   f:write(UNPACK_CHILD)
   f:close()

   local p = io.popen("luajit " .. script .. " 2>/dev/null")
   assert_equals(p:read("*l"), "ready")
   w.u:write(7)
   ubx.cblock_step(pk)
   local res = p:read("*l")
   p:close()
   os.remove(script)
   assert_equals(res, "7")
end

--- re-initializing pack: unpack follows on start if the layout is the
--- same, and refuses to start if it changed
function TestPack:TestPackReinit()
   node("TestPackReinit")
   local pk, w = mkpack(SEQ)
   local up, r = mkunpack(SEQ)

   assert_equals(ubx.block_tostate(up, 'inactive'), 0)
   assert_equals(ubx.block_tostate(pk, 'preinit'), 0)
   assert_equals(ubx.block_tostate(pk, 'inactive'), 0)
   w.seq = ubx.port_clone_conn(pk, "seq")
   assert_equals(ubx.block_tostate(pk, 'active'), 0)
   assert_equals(ubx.block_tostate(up, 'active'), 0)
   produce(pk, w, 42)
   up:do_step()
   assert_equals(num(r.seq), 42)

   assert_equals(ubx.block_tostate(up, 'inactive'), 0)
   assert_equals(ubx.block_tostate(pk, 'preinit'), 0)
   ubx.set_config(pk, "ports", { { id = "seq", type = "uint32_t" } })
   assert_equals(ubx.block_tostate(pk, 'inactive'), 0)
   lu.assert_not_equals(ubx.block_tostate(up, 'active'), 0)
end

--- ubx-pack lists the pack block, and decodes its frames as CSV
function TestPack:TestTool()
   local r = os.execute("command -v ubx-pack >/dev/null 2>&1")
   lu.skipIf(not (r == 0 or r == true), "ubx-pack not found")

   node("TestPackTool")
   local pk, w = mkpack({ { id = "u", type = "uint64_t" },
			  { id = "arr", type = "int32_t", len = 2 },
			  { id = "ts", type = "struct ubx_tstat" },
			  { id = "last", type = "uint64_t" } }, 2)

   local f = io.popen("ubx-pack list")
   local out = f:read("*a")
   f:close()
   lu.assert_str_contains(out, "TestPackTool.pk")
   lu.assert_str_contains(out, "arr:int32_t[2]")

   f = io.popen("ubx-pack read TestPackTool.pk -n 2 2>/dev/null")
   local hdr = f:read("*l")
   lu.assert_str_contains(hdr, "u,arr[0],arr[1],")
   lu.assert_str_contains(hdr, ",ts.cnt,")

   local ncols = #utils.split(hdr, ",")
   w.u:write(42)
   pk:do_step()
   w.arr:write({ 3, 4 })
   w.last:write(99)
   pk:do_step()
   local row1, row2 = f:read("*l"), f:read("*l")
   f:close()

   assert_equals(#utils.split(row1 .. "x", ","), ncols)
   lu.assert_str_matches(row1, "^42,,,.*")

   -- after a port not a multiple of 8 bytes on 32 bit
   lu.assert_str_matches(row2, "^,3,4,.*,99,?$")
end

--- one frame from ubx-pack: new frames only and no header to sync on,
--- so keep producing until it shows up
local function tool_frame(id, format, produce)
   local out = os.tmpname()
   os.execute(string.format("ubx-pack read %s -f %s -n 1 2>/dev/null > %s &",
			    id, format, out))
   local line
   for _ = 1, 200 do
      produce()
      ubx.clock_mono_sleep(0, 50000000)
      local f = io.open(out)
      line = f:read("*l")
      if line and line:sub(1, 2) == "--" then line = f:read("*l") end
      f:close()
      if line then break end
   end
   os.remove(out)
   return line
end

--- ubx-pack encodes a frame as a dictionary of the ports with new data
function TestPack:TestToolDict()
   local r = os.execute("command -v ubx-pack >/dev/null 2>&1")
   lu.skipIf(not (r == 0 or r == true), "ubx-pack not found")

   node("TestPackDict")
   local pk, w = mkpack({ { id = "u", type = "uint64_t" },
			  { id = "arr", type = "int32_t", len = 2 },
			  { id = "d", type = "double" } }, 2)
   local function produce()
      w.u:write(42)
      w.arr:write({ 3, 4 })
      pk:do_step()
   end
   local exp = { u = 42, arr = { 3, 4 } }

   local line = tool_frame("TestPackDict.pk", "plain", produce)
   lu.assert_not_nil(line)
   assert_equals(loadstring("return "..line)(), exp)

   local ok, mod = pcall(require, "cjson")
   if not ok then ok, mod = pcall(require, "json") end
   if ok then
      line = tool_frame("TestPackDict.pk", "json", produce)
      lu.assert_not_nil(line)
      assert_equals(mod.decode(line), exp)
   end
end

--- replaying into a segment an unpack block holds, repeatedly
function TestPack:TestToolReplayInto()
   local r = os.execute("command -v ubx-pack >/dev/null 2>&1")
   lu.skipIf(not (r == 0 or r == true), "ubx-pack not found")

   node("TestPackInto")
   local up = ubx.block_create(nd, "ubx/unpack", "up")
   ubx.do_configure(up, { packid = "in", ports = U64, buffer_len = 8 })
   local rd_p = ubx.port_clone_conn(up, "u", 8)
   assert_equals(ubx.block_tostate(up, 'active'), 0)

   local fn = os.tmpname()
   local f = io.open(fn, "w")
   f:write('--ports={{id="u",len=1,type="uint64_t"}}\n{u=7}\n{u=8}\n')
   f:close()

   for _ = 1, 2 do
      r = os.execute("ubx-pack write TestPackInto.in < "..fn.." 2>/dev/null")
      lu.assert_true(r == 0 or r == true)
      up:do_step()
      assert_equals(num(rd_p), 7)
      up:do_step()
      assert_equals(num(rd_p), 8)
   end

   os.remove(fn)
end

--- a recording replays with the exact values, also with a stopped source
function TestPack:TestToolReplay()
   local r = os.execute("command -v ubx-pack >/dev/null 2>&1")
   lu.skipIf(not (r == 0 or r == true), "ubx-pack not found")

   local rec = table.concat({
      '--ports={{id="u",len=1,type="uint64_t"},{id="d",len=1,type="double"},'
	 ..'{id="arr",len=2,type="int32_t"}}',
      '{u=1152921504606846977ULL,d=0.30000000000000004}',
      '{d=0/0,arr={3,-4}}',
      '{u=3,d=0.1}', "" }, "\n")
   local fn = os.tmpname()
   local f = io.open(fn, "w")
   f:write(rec)
   f:close()

   local out = os.tmpname()
   os.execute("ubx-pack write TestPackReplay.pk -w 2 -r 100 -b 8 < "..fn.." 2>/dev/null &")

   for _ = 1, 150 do
      if io.open("/dev/shm/TestPackReplay.pk") then break end
      ubx.clock_mono_sleep(0, 10000000)
   end

   os.execute("ubx-pack read TestPackReplay.pk -f plain -n 3 > "..out.." 2>/dev/null")
   f = io.open(out)
   local res = f:read("*a")
   f:close()
   os.remove(fn)
   os.remove(out)

   assert_equals(res, rec)
end

--- pack and unpack in two free-running ptrig threads. The ramp writes
--- equal elements counting up: the reader sees them unequal on a torn
--- frame, and not increasing on a reordered or repeated one.
local checker = [[
local ubx = require("ubx")
local ffi = require("ffi")
local p_in, p_stat, sample, val
local last, nread, nerr = 0ULL, 0, 0

function init(b)
   ubx.inport_add(b, "in", "ramp values", 0, "uint64_t", %d)
   ubx.outport_add(b, "stat", "{reads, errors}", 0, "uint64_t", 2)
   p_in = ubx.port_get(b, "in")
   p_stat = ubx.port_get(b, "stat")
   sample = ubx.port_alloc_read_sample(p_in)
   val = ffi.cast("uint64_t*", sample.data)
   return true
end

function step(b)
   local len = ubx.port_read(p_in, sample)
   if len <= 0 then return end
   nread = nread + 1
   for i = 1, tonumber(len) - 1 do
      if val[i] ~= val[0] then nerr = nerr + 1 end
   end
   if val[0] <= last then nerr = nerr + 1 end
   last = val[0]
   ubx.port_write(p_stat, { nread, nerr })
end

function cleanup(b)
   ubx.port_rm(b, "in")
   ubx.port_rm(b, "stat")
end
]]

function TestPack:TestThreads()
   local N = 64
   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "ptrig", "ramp", "luablock", "pack" },
      blocks = {
	 { name = "ramp", type = "ubx/ramp" },
	 { name = "pk", type = "ubx/pack" },
	 { name = "up", type = "ubx/unpack" },
	 { name = "chk", type = "ubx/luablock" },
	 { name = "trigw", type = "ubx/ptrig" },
	 { name = "trigr", type = "ubx/ptrig" },
      },
      configurations = {
	 { name = "ramp", config = { type = "uint64_t", data_len = N, start = 1, slope = 1 } },
	 { name = "pk", config = { packid = "pk", ports = { { id = "v", type = "uint64_t", len = N } }, buffer_len = 4 } },
	 { name = "up", config = { packid = "pk" } },
	 { name = "chk", config = { lua_str = string.format(checker, N) } },
	 { name = "trigw", config = { period_ns = 0, chain0 = { { b = "#ramp" }, { b = "#pk" } } } },
	 { name = "trigr", config = { period_ns = 0, chain0 = { { b = "#up" }, { b = "#chk" } } } },
      },
      connections = {
	 { src = "ramp.out", tgt = "pk.v" },
	 { src = "up.v", tgt = "chk.in" },
      },
   }

   nd = sys:launch({ nodename = "TestPackThreads", loglevel = ffi.C.UBX_LOGLEVEL_WARN, nostart = true })
   lu.assert_not_nil(nd)
   local p_stat = ubx.port_clone_conn(nd:b("chk"), "stat", 1, nil, 7, 0)
   sys:startup(nd)
   ubx.clock_mono_sleep(0, 300000000)
   nd:b("trigr"):do_stop()
   nd:b("trigw"):do_stop()

   local len, stat = p_stat:read()
   assert_equals(tonumber(len), 2)
   stat = stat:tolua()
   -- with one cpu the two threads alternate, at a few frames per timeslice
   lu.assert_true(tonumber(stat[1]) > 10, "too few reads: "..tonumber(stat[1]))
   assert_equals(tonumber(stat[2]), 0)
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
