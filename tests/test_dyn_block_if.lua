
local ffi=require"ffi"
local lunit=require"lunit"
local ubx=require"ubx"
local utils=require"utils"
local cdata=require"cdata"

module("test_dyn_block_if", lunit.testcase, package.seeall)

ni=ubx.node_create("unit_test_node")
ubx.load_module(ni, "std_types/stdtypes/stdtypes.so")
ubx.load_module(ni, "std_types/testtypes/testtypes.so")
ubx.load_module(ni, "std_blocks/luablock/luablock.so")
ubx.ffi_load_types(ni)

function test_port_add_rm()
   
end

function test_config_add_rm()

end