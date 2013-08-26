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

local lua_testcomp = [[
ubx=require "ubx"
ffi=require "ffi"
local rd, this

function init(b)
   b=ffi.cast("ubx_block_t*", b)
   this=b
   ubx.ffi_load_types(b.ni)

   -- print("adding port 'pos_in' to block ", ubx.safe_tostr(b.name))
   ubx.port_add(b, "pos_in",
		  "{ desc='current measured position' }",
		  "testtypes/struct Vector", 1, nil, 0, 0)

   ubx.port_add(b, "pos_out",
		  "{ desc='desired position' }",
		  nil, 0, "testtypes/struct Vector", 1, 0)

   rd = ubx.data_alloc(b.ni, "testtypes/struct Vector")
   return true
end

function start(b)
   return true
end

function step(b)
   local res = ubx.port_read(ubx.port_get(b, "pos_in"), rd)
   if res<=0 then
       print("step: no sample received", res)
       goto out
   end
   local cdat=ubx.data_to_cdata(rd)
   -- print("received vector["..tostring(res).."]: ", cdat.x, cdat.y, cdat.z)
   cdat.x=cdat.x*2; cdat.y=cdat.y*2; cdat.z=cdat.z*2;
   ubx.port_write(ubx.port_get(b, "pos_out"), rd)
   ::out::
end

function cleanup(b)
   ubx.port_rm(b, "pos")
end
]]


lb1=ubx.block_create(ni, "lua/luablock", "lb1", { lua_str=lua_testcomp } )
fifo1=ubx.block_create(ni, "lfds_buffers/cyclic", "fifo1", {element_num=4, element_size=code_str_len})

fifo_in=ubx.block_create(ni, "lfds_buffers/cyclic", "fifo_in",
			 {element_num=4, element_size=ubx.type_size(ni, "testtypes/struct Vector")})
fifo_out=ubx.block_create(ni, "lfds_buffers/cyclic", "fifo_out",
			  {element_num=4, element_size=ubx.type_size(ni, "testtypes/struct Vector")})

assert(ubx.block_init(lb1)==0)
assert(ubx.block_init(fifo1)==0)
assert(ubx.block_init(fifo_in)==0)
assert(ubx.block_init(fifo_out)==0)

p_exec_str=ubx.port_get(lb1, "exec_str")
p_pos_in=ubx.port_get(lb1, "pos_in")
p_pos_out=ubx.port_get(lb1, "pos_out")

ubx.connect_one(p_exec_str, fifo1)
ubx.connect_one(p_pos_in, fifo_in)
ubx.connect_one(p_pos_out, fifo_out)

assert(ubx.block_start(lb1)==0)
assert(ubx.block_start(fifo1)==0)
assert(ubx.block_start(fifo_in)==0)
assert(ubx.block_start(fifo_out)==0)

local d1=ubx.data_alloc(ni, "char")
local d2=ubx.data_alloc(ni, "int")
local _vin=ubx.data_alloc(ni, "testtypes/struct Vector")
local _vout=ubx.data_alloc(ni, "testtypes/struct Vector")
vcdin = ubx.data_to_cdata(_vin)
vcdout = ubx.data_to_cdata(_vout)

ubx.data_set(_vin, {x=1,y=2,z=3})

--- call helper
function exec_str(str)
   ubx.data_set(d1, str, true) -- resize if necessary!
   ubx.interaction_write(fifo1, d1)
   ubx.cblock_step(lb1)
   local res=ubx.interaction_read(fifo1, d2)
   if res<=0 then error("no response from exec_str") end
   return ubx.data_tolua(d2)
end

function test_comm()
   for i=1,10 do
      vcdin.x=vcdin.x*i; vcdin.y=vcdin.y*i; vcdin.z=vcdin.z*i;
      ubx.interaction_write(fifo_in, _vin)
      ubx.cblock_step(lb1)
      assert_true(ubx.interaction_read(fifo_out, _vout) > 0, "test block produced no output")
      assert_equal(vcdin.x*2, vcdout.x)
      assert_equal(vcdin.y*2, vcdout.y)
      assert_equal(vcdin.z*2, vcdout.z)
   end
end
