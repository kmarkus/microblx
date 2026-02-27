
local ffi=require"ffi"
local lu=require"luaunit"
local ubx=require"ubx"

local assert_equals = lu.assert_equals
local tn = tonumber

local ubx_timespec=ffi.typeof("struct ubx_timespec")



-- | 1.sec | 1.nsec | 2.sec | 2.nsec | nsec wrap |
-- |-------+--------+-------+--------+-----------|
-- | POS   | POS    | POS   | POS    |           |
-- |       |        |       |        |           |
-- |       |        |       |        |           |

TestTimeArith = {}

-- POS POS
function TestTimeArith:test_sub_pos_sec_pos_nsec()
   local ts1= ubx_timespec{sec=3, nsec=2}
   local ts2= ubx_timespec{sec=2, nsec=1}
   local tsres = ubx_timespec()
   ubx.ts_sub(ts1, ts2, tsres)
   assert_equals(1, tn(tsres.sec))
   assert_equals(1, tn(tsres.nsec))
end

-- NULL POS
function TestTimeArith:test_sub_null_sec_pos_nsec()
   local ts1= ubx_timespec{sec=3, nsec=1}
   local ts2= ubx_timespec{sec=2, nsec=2}
   local tsres = ubx_timespec()
   ubx.ts_sub(ts1, ts2, tsres)
   assert_equals(0, tn(tsres.sec))
   assert_equals(999999999, tn(tsres.nsec))
end

-- NULL NEG
function TestTimeArith:test_sub_null_sec_neg_nsec()
   local ts1= ubx_timespec{sec=2, nsec=2}
   local ts2= ubx_timespec{sec=3, nsec=1}
   local tsres = ubx_timespec()
   ubx.ts_sub(ts1, ts2, tsres)
   assert_equals(0, tn(tsres.sec))
   assert_equals(-999999999, tn(tsres.nsec))
end

--- POS NEG
function TestTimeArith:test_sub_pos_sec_neg_nsec()
   local ts1= ubx_timespec{sec=4, nsec=1}
   local ts2= ubx_timespec{sec=2, nsec=2}
   local tsres = ubx_timespec()
   ubx.ts_sub(ts1, ts2, tsres)
   assert_equals(1, tn(tsres.sec))
   assert_equals(999999999, tn(tsres.nsec))
end

-- NULL sec, NEG nsec (ts1 < ts2, small diff)
function TestTimeArith:test_sub_null_sec_neg_nsec_2()
   local ts1= ubx_timespec{sec=2, nsec=2}
   local ts2= ubx_timespec{sec=3, nsec=1}
   local tsres = ubx_timespec()
   ubx.ts_sub(ts1, ts2, tsres)
   assert_equals(0, tn(tsres.sec))
   assert_equals(-999999999, tn(tsres.nsec))
end

-- NEG sec, NEG nsec (ts1 << ts2)
function TestTimeArith:test_sub_neg_sec_neg_nsec_wrap()
   local ts1= ubx_timespec{sec=2, nsec=2}
   local ts2= ubx_timespec{sec=4, nsec=1}
   local tsres = ubx_timespec()
   ubx.ts_sub(ts1, ts2, tsres)
   assert_equals(-1, tn(tsres.sec))
   assert_equals(-999999999, tn(tsres.nsec))
end


-- NEG sec, NEG nsec (both components negative)
function TestTimeArith:test_sub_neg_sec_neg_nsec()
   local ts1= ubx_timespec{sec=2, nsec=1}
   local ts2= ubx_timespec{sec=3, nsec=2}
   local tsres = ubx_timespec()
   ubx.ts_sub(ts1, ts2, tsres)
   assert_equals(-1, tn(tsres.sec))
   assert_equals(-1, tn(tsres.nsec))
end


-- Addition
function TestTimeArith:test_add_pos_sec_pos_nsec()
   local ts1= ubx_timespec{sec=2, nsec=500000000}
   local ts2= ubx_timespec{sec=1, nsec=500000001}
   local tsres = ubx_timespec()
   ubx.ts_add(ts1, ts2, tsres)
   assert_equals(4, tn(tsres.sec))
   assert_equals(1, tn(tsres.nsec))
end

-- Addition
function TestTimeArith:test_add_zero_sec_neg_nsec()
   local ts1= ubx_timespec{sec=2, nsec=500000000}
   local ts2= ubx_timespec{sec=0, nsec=-500000001}
   local tsres = ubx_timespec()
   ubx.ts_add(ts1, ts2, tsres)
   assert_equals(1, tn(tsres.sec))
   assert_equals(999999999, tn(tsres.nsec))
end

-- Addition
function TestTimeArith:test_add_neg_sec_neg_nsec()
   local ts1= ubx_timespec{sec=1, nsec=500000000}
   local ts2= ubx_timespec{sec=-2, nsec=-500000001}
   local tsres = ubx_timespec()
   ubx.ts_add(ts1, ts2, tsres)
   assert_equals(-1, tn(tsres.sec))
   assert_equals(-1, tn(tsres.nsec))
end

--- ts_div tests
function TestTimeArith:test_div_basic()
   local ts1= ubx_timespec{sec=4, nsec=0}
   local tsres = ubx_timespec()
   ubx.ts_div(ts1, 2, tsres)
   assert_equals(2, tn(tsres.sec))
   assert_equals(0, tn(tsres.nsec))
end

function TestTimeArith:test_div_with_remainder()
   local ts1= ubx_timespec{sec=1, nsec=0}
   local tsres = ubx_timespec()
   ubx.ts_div(ts1, 3, tsres)
   -- 1e9 / 3 = 333333333.33
   assert_equals(0, tn(tsres.sec))
   assert_equals(333333333, tn(tsres.nsec))
end

function TestTimeArith:test_div_by_zero()
   local ts1= ubx_timespec{sec=5, nsec=123}
   local tsres = ubx_timespec()
   ubx.ts_div(ts1, 0, tsres)
   assert_equals(0, tn(tsres.sec))
   assert_equals(0, tn(tsres.nsec))
end

function TestTimeArith:test_div_nsec()
   local ts1= ubx_timespec{sec=0, nsec=900000000}
   local tsres = ubx_timespec()
   ubx.ts_div(ts1, 3, tsres)
   assert_equals(0, tn(tsres.sec))
   assert_equals(300000000, tn(tsres.nsec))
end

--- ts_cmp tests
function TestTimeArith:test_cmp_equal()
   local ts1= ubx_timespec{sec=3, nsec=100}
   local ts2= ubx_timespec{sec=3, nsec=100}
   assert_equals(0, ubx.ts_cmp(ts1, ts2))
end

function TestTimeArith:test_cmp_greater_sec()
   local ts1= ubx_timespec{sec=4, nsec=0}
   local ts2= ubx_timespec{sec=3, nsec=999999999}
   assert_equals(1, ubx.ts_cmp(ts1, ts2))
end

function TestTimeArith:test_cmp_less_sec()
   local ts1= ubx_timespec{sec=2, nsec=999999999}
   local ts2= ubx_timespec{sec=3, nsec=0}
   assert_equals(-1, ubx.ts_cmp(ts1, ts2))
end

function TestTimeArith:test_cmp_greater_nsec()
   local ts1= ubx_timespec{sec=3, nsec=200}
   local ts2= ubx_timespec{sec=3, nsec=100}
   assert_equals(1, ubx.ts_cmp(ts1, ts2))
end

function TestTimeArith:test_cmp_less_nsec()
   local ts1= ubx_timespec{sec=3, nsec=100}
   local ts2= ubx_timespec{sec=3, nsec=200}
   assert_equals(-1, ubx.ts_cmp(ts1, ts2))
end

--- ts_to_double test
function TestTimeArith:test_ts_to_double()
   local ts1= ubx_timespec{sec=3, nsec=500000000}
   local d = tn(ubx.ts_to_double(ts1))
   assert(math.abs(d - 3.5) < 1e-9, "ts_to_double: expected 3.5 got "..d)
end

--- ts_to_ns test
function TestTimeArith:test_ts_to_ns()
   local ts1= ubx_timespec{sec=1, nsec=500000000}
   assert_equals(1500000000ULL, ubx.ts_to_ns(ts1))
end

--- ts_to_us test
function TestTimeArith:test_ts_to_us()
   local ts1= ubx_timespec{sec=1, nsec=500000000}
   assert_equals(1500000ULL, ubx.ts_to_us(ts1))
end

--- ts_norm tests
function TestTimeArith:test_norm_large_nsec()
   local ts = ubx_timespec{sec=1, nsec=2000000000}
   ubx.ts_norm(ts)
   assert_equals(3, tn(ts.sec))
   assert_equals(0, tn(ts.nsec))
end

function TestTimeArith:test_norm_neg_nsec()
   local ts = ubx_timespec{sec=3, nsec=-1500000000}
   ubx.ts_norm(ts)
   assert_equals(1, tn(ts.sec))
   assert_equals(500000000, tn(ts.nsec))
end

--- timespec metatype operator tests
function TestTimeArith:test_mt_eq()
   local ts1= ubx_timespec{sec=3, nsec=100}
   local ts2= ubx_timespec{sec=3, nsec=100}
   lu.assert_true(ts1 == ts2)
end

function TestTimeArith:test_mt_lt()
   local ts1= ubx_timespec{sec=2, nsec=100}
   local ts2= ubx_timespec{sec=3, nsec=100}
   lu.assert_true(ts1 < ts2)
   lu.assert_false(ts2 < ts1)
end

function TestTimeArith:test_mt_le()
   local ts1= ubx_timespec{sec=3, nsec=100}
   local ts2= ubx_timespec{sec=3, nsec=100}
   lu.assert_true(ts1 <= ts2)
   local ts3= ubx_timespec{sec=2, nsec=100}
   lu.assert_true(ts3 <= ts1)
end

if not _RUNNER then os.exit( lu.LuaUnit.run() ) end
