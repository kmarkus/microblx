#!/usr/bin/luajit

local lu=require"luaunit"
local ffi=require"ffi"
local ubx=require"ubx"
local utils=require"utils"
local cdata=require"cdata"

assert_equals = lu.assert_equals
assert_true = lu.assert_true

-- require"trace"
-- require"strict"

local code_str_len = 16*1024*1024

local nd = ubx.node_create("test_struct_comm")

ubx.load_module(nd, "stdtypes")
ubx.load_module(nd, "testtypes")
ubx.load_module(nd, "luablock")
ubx.load_module(nd, "lfds_cyclic")

local lua_testcomp = [[
ubx=require "ubx"
ffi=require "ffi"
local rd, this

function init(b)
   b=ffi.cast("ubx_block_t*", b)
   this=b
   ubx.ffi_load_types(b.nd)

   -- print("adding port 'pos_in' to block ", ubx.safe_tostr(b.name))
   ubx.port_add(b, "pos_in", "{ desc='current measured position' }",
                0, "struct kdl_vector", 1, nil, 0)

   ubx.port_add(b, "pos_out", "{ desc='desired position' }",
                0, nil, 0, "struct kdl_vector", 1)

   rd = ubx.data_alloc(b.nd, "struct kdl_vector")
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


lb1=ubx.block_create(nd, "ubx/luablock", "lb1", { lua_str=lua_testcomp } )
assert_equals(ubx.block_init(lb1), 0)

local p_pos_out = ubx.port_clone_conn(lb1, "pos_in", 4)
local p_pos_in = ubx.port_clone_conn(lb1, "pos_out", 4)

assert_equals(ubx.block_start(lb1), 0)

local _vin=ubx.data_alloc(nd, "struct kdl_vector")
local _vout=ubx.data_alloc(nd, "struct kdl_vector")
vcdin = ubx.data_to_cdata(_vin)
vcdout = ubx.data_to_cdata(_vout)

ubx.data_set(_vin, {x=1,y=2,z=3})

function test_comm()
   for i=1,100000 do
      vcdin.x=vcdin.x*i; vcdin.y=vcdin.y*i; vcdin.z=vcdin.z*i;
      ubx.port_write(p_pos_out, _vin)
      ubx.cblock_step(lb1)
      assert_true(ubx.port_read(p_pos_in, _vout) > 0, "test block produced no output")
      assert_equals(vcdin.x*2, vcdout.x)
      assert_equals(vcdin.y*2, vcdout.y)
      assert_equals(vcdin.z*2, vcdout.z)
   end
end

os.exit( lu.LuaUnit.run() )
