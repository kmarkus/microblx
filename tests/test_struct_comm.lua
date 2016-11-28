#!/usr/bin/luajit

local ffi=require"ffi"
local lunit=require"lunit"
local ubx=require"ubx"
local utils=require"utils"
local cdata=require"cdata"
-- require"trace"
-- require"strict"

local code_str_len = 16*1024*1024

module("test_struct_comm", lunit.testcase, package.seeall)

local ni = ubx.node_create("test_struct_comm")

ubx.load_module(ni, "stdtypes")
ubx.load_module(ni, "kdl_types")
ubx.load_module(ni, "luablock")
ubx.load_module(ni, "lfds_cyclic")

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
		  "struct kdl_vector", 1, nil, 0, 0)

   ubx.port_add(b, "pos_out",
		  "{ desc='desired position' }",
		  nil, 0, "struct kdl_vector", 1, 0)

   rd = ubx.data_alloc(b.ni, "struct kdl_vector")
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
   ubx.port_rm(b, "pos_out")
   ubx.port_rm(b, "pos_in")
end
]]


lb1=ubx.block_create(ni, "lua/luablock", "lb1", { lua_str=lua_testcomp } )
assert(ubx.block_init(lb1)==0)

local p_pos_out = ubx.port_clone_conn(lb1, "pos_in", 4)
local p_pos_in = ubx.port_clone_conn(lb1, "pos_out", 4)

assert(ubx.block_start(lb1)==0)

local _vin=ubx.data_alloc(ni, "struct kdl_vector")
local _vout=ubx.data_alloc(ni, "struct kdl_vector")
vcdin = ubx.data_to_cdata(_vin)
vcdout = ubx.data_to_cdata(_vout)

ubx.data_set(_vin, {x=1,y=2,z=3})

function test_comm()
   for i=1,10 do
      vcdin.x=vcdin.x*i; vcdin.y=vcdin.y*i; vcdin.z=vcdin.z*i;
      ubx.port_write(p_pos_out, _vin)
      ubx.cblock_step(lb1)
      assert_true(ubx.port_read(p_pos_in, _vout) > 0, "test block produced no output")
      assert_equal(vcdin.x*2, vcdout.x)
      assert_equal(vcdin.y*2, vcdout.y)
      assert_equal(vcdin.z*2, vcdout.z)
   end
end
