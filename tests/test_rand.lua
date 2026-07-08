--
-- Tests for the runtime-typed random generator (ubx/rand)
--
-- Focus: per-instance PRNG state (reproducibility, instance
-- independence), the [0,1) float range, data_len vectors and the
-- legacy drand48 sequence compatibility of the seeding.
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

TestRand = {}

function TestRand:teardown()
   if ni then ubx.node_rm(ni) end
   ni = nil
end

-- launch one system with N identically or differently configured rand
-- blocks named r1..rN; returns their (block, out-port) pairs
local function make(cfgs, nodename)
   local blocks, configurations = {}, {}
   for i, cfg in ipairs(cfgs) do
      blocks[i] = { name = "r" .. i, type = "ubx/rand" }
      configurations[i] = { name = "r" .. i, config = cfg }
   end

   local sys = bd.system {
      imports = { "stdtypes", "lfrb", "rand" },
      blocks = blocks,
      configurations = configurations,
   }

   ni = sys:launch({ nodename = nodename, nostart = true, loglevel = LOGLEVEL })
   assert_not_nil(ni)

   local res = {}
   for i = 1, #cfgs do
      local b = ni:b("r" .. i)
      local p = ubx.port_clone_conn(b, "out", nil, 1, 7, 0)
      ubx.block_tostate(b, 'active')
      res[i] = { b = b, p = p }
   end
   return res
end

local function sample(r)
   r.b:do_step()
   local len, val = r.p:read()
   assert_true(tonumber(len) > 0, "expected output")
   return val:tolua()
end

--- doubles are uniform in [0,1)
function TestRand:TestDoubleRange()
   local r = make({ { type = "double", seed = 42 } }, "TestRandRange")[1]

   for _ = 1, 100 do
      local v = sample(r)
      assert_true(v >= 0 and v < 1, "value out of [0,1): " .. v)
   end
end

--- same seed => same sequence; different seed => different sequence
function TestRand:TestReproducible()
   local rs = make({
	 { type = "double", seed = 42 },
	 { type = "double", seed = 42 },
	 { type = "double", seed = 43 },
   }, "TestRandRepro")

   local differs = false
   for _ = 1, 10 do
      local a, b, c = sample(rs[1]), sample(rs[2]), sample(rs[3])
      assert_equals(b, a)	-- identical seeds run in lockstep
      differs = differs or (a ~= c)
   end
   assert_true(differs, "seed 43 should produce a different sequence")
end

--- per-instance state: stepping one block does not disturb another
--- (with the global-state legacy variants r1 and r2 would interleave)
function TestRand:TestInstanceIndependence()
   local rs = make({
	 { type = "double", seed = 7 },
	 { type = "double", seed = 7 },
   }, "TestRandIndep")

   -- advance r1 a few extra times in between; r2 must be unaffected
   local seq1, seq2 = {}, {}
   for i = 1, 5 do seq1[i] = sample(rs[1]) end
   for i = 1, 5 do seq2[i] = sample(rs[2]) end
   assert_equals(seq2, seq1)
end

--- seeding matches the legacy srand48/drand48 sequence
function TestRand:TestLegacyCompatSequence()
   local r = make({ { type = "double", seed = 42 } }, "TestRandLegacy")[1]

   -- reference: srand48(42); drand48() x 3, computed with glibc
   ffi.cdef[[ void srand48(long seedval); double drand48(void); ]]
   ffi.C.srand48(42)
   for _ = 1, 3 do
      local exp = ffi.C.drand48()
      assert_true(math.abs(sample(r) - exp) < 1e-15,
		  "sequence deviates from srand48/drand48")
   end
end

--- data_len > 1 yields a vector of independent values
function TestRand:TestVector()
   local r = make({ { type = "uint32_t", data_len = 4, seed = 1 } },
		  "TestRandVec")[1]

   local v = sample(r)
   assert_equals(#v, 4)
   -- 4 identical values would be a (vanishingly unlikely) failure
   assert_true(not (v[1] == v[2] and v[2] == v[3] and v[3] == v[4]),
	       "vector elements should differ")
end

--- integer types produce values; int8 stays in range
function TestRand:TestInt8()
   local r = make({ { type = "int8_t", seed = 5 } }, "TestRandI8")[1]

   for _ = 1, 50 do
      local v = sample(r)
      assert_true(v >= -128 and v <= 127, "int8 out of range: " .. v)
   end
end

--- an unsupported type is refused at init
function TestRand:TestBadType()
   assert_true(not pcall(make, { { type = "char" } }, "TestRandBadType"))
end

if not _RUNNER then os.exit(lu.LuaUnit.run()) end
