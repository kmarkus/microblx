#!/usr/bin/luajit

local ffi=require"ffi"
local lunit=require"lunit"
local ubx=require"ubx"
local utils=require"utils"
local cdata=require"cdata"
--require"trace"

local code_str_len = 16*1024*1024

module("test_struct_comm", lunit.testcase, package.seeall)

local ni = ubx.node_create("test_struct_comm")

ubx.load_module(ni, "std_types/stdtypes/stdtypes.so")
ubx.load_module(ni, "std_types/testtypes/testtypes.so")
ubx.load_module(ni, "std_blocks/luablock/luablock.so")
ubx.load_module(ni, "std_blocks/lfds_buffers/lfds_cyclic.so")

local lua_testcomp = [=[
ubx=require "ubx"
ffi=require "ffi"
local rd, this

function init(b)
   b=ffi.cast("ubx_block_t*", b)
   this=b
   ubx.ffi_load_types(b.ni)

   print("adding port 'pos' to block ", ubx.safe_tostr(b.name))
   ubx.port_add(b, "pos",
		  "{ desc='current measured position' }",
		  "testtypes/struct Vector", 1, nil, 0, 0)

   rd = ubx.data_alloc(b.ni, "testtypes/struct Vector")
   return true
end

function start(b)
   return true
end


function step(b)
   local res = ubx.port_read(ubx.port_get(b, "pos"), rd)
   if res<=0 then error("step: no sample received") end
   local cdat=ubx.data_to_ctype(d)
   print("vector: ", cdat.x, cdat.y, cdat.z)
end

function cleanup(b)
   ubx.port_rm(b, "pos")
end
]=]


lb1=ubx.block_create(ni, "lua/luablock", "lb1", { lua_str=lua_testcomp } )
fifo1=ubx.block_create(ni, "lfds_buffers/cyclic", "fifo1", {element_num=4, element_size=code_str_len})
fifo2=ubx.block_create(ni, "lfds_buffers/cyclic", "fifo12", {element_num=4, element_size=ubx.type_size(ni, "testtypes/struct Vector")})

p_exec_str=ubx.port_get(lb1, "exec_str")
ubx.connect_one(p_exec_str, fifo1)

assert(ubx.block_init(lb1)==0)
assert(ubx.block_init(fifo1)==0)
assert(ubx.block_start(lb1)==0)
assert(ubx.block_start(fifo1)==0)

local d1=ubx.data_alloc(ni, "char")
local d2=ubx.data_alloc(ni, "int")

--- call helper
function exec_str(str)
   ubx.data_set(d1, str, true) -- resize if necessary!
   ubx.interaction_write(fifo1, d1)
   ubx.cblock_step(lb1)
   local res=ubx.interaction_read(fifo1, d2)
   if res<=0 then error("no response from exec_str") end
   return ubx.data_tolua(d2)
end

function test_exec_str()
end


function test_comm()
   
   
end
