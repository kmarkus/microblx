#!/usr/bin/luajit

local ffi=require"ffi"
local lunit=require"lunit"
local ubx=require"ubx"
local utils=require"utils"
local cdata=require"cdata"
-- require"trace"

local code_str_len = 16*1024*1024

module("test_dyn_block_if", lunit.testcase, package.seeall)

local ni=ubx.node_create("test_dyn_block_if")

ubx.load_module(ni, "stdtypes")
ubx.load_module(ni, "testtypes")
ubx.load_module(ni, "kdl_types")
ubx.load_module(ni, "luablock")
ubx.load_module(ni, "lfds_cyclic")

lb1=ubx.block_create(ni, "lua/luablock", "lb1")
p_exec_str=ubx.port_get(lb1, "exec_str")
fifo1=ubx.block_create(ni, "lfds_buffers/cyclic", "fifo1", {element_num=4, element_size=code_str_len})

ubx.port_connect_out(p_exec_str, fifo1);
ubx.port_connect_in(p_exec_str, fifo1);

assert(ubx.block_init(lb1)==0)
assert(ubx.block_init(fifo1)==0)
assert(ubx.block_start(lb1)==0)
assert(ubx.block_start(fifo1)==0)

local d1=ubx.data_alloc(ni, "char")
local d2=ubx.data_alloc(ni, "int")


--- helper
function exec_str(str)
   ubx.data_set(d1, str, true) -- resize if necessary!
   ubx.interaction_write(fifo1, d1)
   ubx.cblock_step(lb1)
   local res=ubx.interaction_read(fifo1, d2)
   if res<=0 then error("no response from exec_str") end
   return ubx.data_tolua(d2)
end

function test_exec_str()
   assert_equal(0, exec_str("counter=75"))
   assert_equal(0, exec_str("return counter==75"))
   -- assert_equal(0, exec_str("print('counter:', counter)"))
   assert_not_equal(0, exec_str("error('this error is intentional')"))

   assert_equal(0, exec_str([[
				  function start(b) this=b; return true end
			    ]]))
   ubx.block_stop(lb1)
   ubx.block_start(lb1)
end

function print_pstate(b)
   local ptab = ubx.ports_map(b, ubx.port_totab)
   for i,v in ipairs(ptab) do
      print("port #"..tostring(i)..": "..utils.tab2str(v))
   end
end

function test_port_add_rm()
   local ptab_before = ubx.ports_map(lb1, ubx.port_totab)

   assert(0, exec_str([[
			    ubx=require "ubx"

			    for k=1,3 do
			       ubx.port_add(this,
					    "testport"..tostring(k),
					    -- "{ desc='a testport without further meaning "..tostring(k).."' }",
					    "",
					    "int32_t", 5, "size_t", 1, 0)
			    end
		      ]]))

   assert_true(ubx.port_get(lb1, "testport0")==nil, "retrieving non-existing port")

   assert_not_nil(ubx.port_get(lb1, "testport1"))
   assert_not_nil(ubx.port_get(lb1, "testport2"))
   assert_not_nil(ubx.port_get(lb1, "testport3"))

   assert_equal(0, exec_str("ubx.port_rm(this, 'testport1')"))
   assert_equal(0, exec_str("ubx.port_rm(this, 'testport3')"))
   assert_equal(0, exec_str("ubx.port_rm(this, 'testport2')"))

   local ptab_after = ubx.ports_map(lb1, ubx.port_totab)
   assert_true(utils.table_cmp(ptab_before, ptab_after), "interface not the same after removing all added ports")
end

function test_config_add_rm()
   local ptab_before = ubx.configs_map(lb1, ubx.config_totab)

   assert(0, exec_str([[
			    ubx=require "ubx"

			    for k=1,3 do
			       ubx.config_add(this,
					      "testconfig"..tostring(k),
					      "{ desc='a testconfig without further meaning' }",
					      "int32_t", 5)
			    end
		      ]]))



   assert_true(ubx.config_get(lb1, "testconfig0")==nil)

   assert_not_nil(ubx.config_get(lb1, "testconfig1"))
   assert_not_nil(ubx.config_get(lb1, "testconfig2"))
   assert_not_nil(ubx.config_get(lb1, "testconfig3"))

   assert_equal(0, exec_str("ubx.config_rm(this, 'testconfig1')"))
   assert_equal(0, exec_str("ubx.config_rm(this, 'testconfig3')"))
   assert_equal(0, exec_str("ubx.config_rm(this, 'testconfig2')"))

   local ptab_after = ubx.configs_map(lb1, ubx.config_totab)

   assert_true(utils.table_cmp(ptab_before, ptab_after), "interface comparison failed")

end
