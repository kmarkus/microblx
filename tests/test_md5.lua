
local ffi=require"ffi"
local lunit=require"lunit"
local ubx=require"ubx"
local utils=require"utils"

module("md5_test", lunit.testcase, package.seeall)

-- $ echo -n "microblx rocks" | md5sum
-- 825a27eaa56c18fd1f4c9c077b497704  -
function test_md5sum_microblx_rocks()
   local res = ubx.md5("microblx rocks")
   assert_equal("825a27eaa56c18fd1f4c9c077b497704", res)
end

function test_md5sum_empty()
   local res = ubx.md5("")
   assert_equal("d41d8cd98f00b204e9800998ecf8427e", res)
end


poe = [[
Once upon a midnight dreary, while I pondered weak and weary,
Over many a quaint and curious volume of forgotten lore,
While I nodded, nearly napping, suddenly there came a tapping,
As of some one gently rapping, rapping at my chamber door.
`'Tis some visitor,' I muttered, `tapping at my chamber door -
Only this, and nothing more.'
]]

function test_md5sum_poe()
   local res = ubx.md5(poe)
   assert_equal("306e4af7c5a27f50705e051b2753ab29", res)
end

