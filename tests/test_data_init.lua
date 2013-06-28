ffi=require"ffi"
lunit=require"lunit"
u5c=require"u5c"

module("data_init_test", lunit.testcase, package.seeall)

local ni
function setup()
   ni=u5c.node_create("unit_test_node")
   u5c.load_module(ni, "std_types/stdtypes/stdtypes.so")
end

function teardown()
   u5c.node_cleanup(ni)
end

function test_scalar_assignmnet()
   local d=u5c.data_alloc(ni, "unsigned int")
   u5c.data_set(d, 33)
   local numptr = ffi.cast("unsigned int*", d.data)
   assert_equal(33, numptr[0])
end


