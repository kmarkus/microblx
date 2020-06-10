#!/usr/bin/luajit

local lu = require("luaunit")
local ubx = require("ubx")
local utils = require("utils")
local ffi = require("ffi")
local bit = require("bit")

local assert_true = lu.assert_true
local assert_false = lu.assert_false
local assert_equals = lu.assert_equals
local assert_not_equals = lu.assert_not_equals
local assert_not_nil = lu.assert_not_nil
local assert_nil = lu.assert_nil

local code_str_len = 16*1024*1024

local ni = ubx.node_create("test_dyn_block_if")



ubx.load_module(ni, "stdtypes")
ubx.load_module(ni, "testtypes")
ubx.load_module(ni, "luablock")
ubx.load_module(ni, "lfds_cyclic")
ubx.load_module(ni, "random")

TestDynIF = {}

local exec_str, lb

function TestDynIF:setup()
   lb = ubx.block_create(ni, "lua/luablock", "lb1",
			 { lua_str = "ubx = require('ubx'); this=nil; function start(b) this=b; return true end" })
   assert_not_nil(lb)

   local p_exec_str = ubx.port_clone_conn(lb, "exec_str", 4, 4)
   local d1=ubx.data_alloc(ni, "char")
   local d2=ubx.data_alloc(ni, "int")

   assert_equals(ubx.block_init(lb), 0)
   assert_equals(ubx.block_start(lb), 0)

   exec_str = function (str)
      ubx.data_set(d1, str, true) -- resize if necessary!
      ubx.port_write(p_exec_str, d1)
      ubx.cblock_step(lb)
      local res = ubx.port_read(p_exec_str, d2)
      if res <= 0 then error("no response from exec_str") end
      return ubx.data_tolua(d2)
   end
end

function TestDynIF:teardown()
   exec_str = nil
   ubx.node_clear(ni)
end

function TestDynIF:TestExecStr()
   assert_equals(0, exec_str("counter=75"))
   assert_equals(0, exec_str("return counter==75"))
   assert_not_equals(0, exec_str("error('this error is intentional')"))
end

function TestDynIF:TestSetHook()
   assert_equals(0, exec_str([[
that=nil
function start(b) that=b; return true end
]]))
   assert_equals(ubx.block_stop(lb), 0)
   assert_equals(ubx.block_start(lb), 0)
   assert_equals(0, exec_str("return that~=nil"))
end

function TestDynIF:TestPortAddRm()
   local ptab_before = ubx.ports_map(lb, ubx.port_totab)

   assert_equals(0, exec_str([[
			    ubx=require "ubx"

			    for k=1,3 do
			       ubx.port_add(this,
					    "testport"..tostring(k),
					    -- "{ desc='a testport without further meaning "..tostring(k).."' }",
					    "",
                                            0,
					    "int32_t", 5, "size_t", 1)
			    end
		      ]]))

   assert_false(pcall(ubx.port_get, lb, "testport0"), "retrieving non-existing port")

   assert_not_nil(ubx.port_get(lb, "testport1"))
   assert_not_nil(ubx.port_get(lb, "testport2"))
   assert_not_nil(ubx.port_get(lb, "testport3"))

   assert_not_nil(ubx.port_get(lb, "testport1").block, "testport1 is missing block ptr")
   assert_not_nil(ubx.port_get(lb, "testport2").block, "testport2 is missing block ptr")
   assert_not_nil(ubx.port_get(lb, "testport3").block, "testport3 is missing block ptr")

   assert_equals(0, exec_str("ubx.port_rm(this, 'testport1')"))
   assert_equals(0, exec_str("ubx.port_rm(this, 'testport3')"))
   assert_equals(0, exec_str("ubx.port_rm(this, 'testport2')"))

   local ptab_after = ubx.ports_map(lb, ubx.port_totab)

   assert_true(utils.table_cmp(ptab_before, ptab_after),
	       "interface not the same after removing all added ports")
end

function TestDynIF:TestConfigAddRm()
   local ptab_before = ubx.configs_map(lb, ubx.config_totab)

   assert_equals(0, exec_str([[
			    ubx=require "ubx"

			    for k=1,3 do
			       ubx.config_add(this,
					      "testconfig"..tostring(k),
					      "{ desc='a testconfig without further meaning' }",
					      "int32_t")
			    end
		      ]]))



   assert_nil(ubx.config_get(lb, "testconfig0"))

   assert_not_nil(ubx.config_get(lb, "testconfig1"))
   assert_not_nil(ubx.config_get(lb, "testconfig2"))
   assert_not_nil(ubx.config_get(lb, "testconfig3"))

   assert_equals(0, exec_str("ubx.config_rm(this, 'testconfig1')"))
   assert_equals(0, exec_str("ubx.config_rm(this, 'testconfig3')"))
   assert_equals(0, exec_str("ubx.config_rm(this, 'testconfig2')"))

   local ptab_after = ubx.configs_map(lb, ubx.config_totab)

   assert_true(utils.table_cmp(ptab_before, ptab_after),
	       "interface not the same after removing all added configs")

end

function TestDynIF:TestConfigAttrCloned()
   local cstatic = ubx.config_get(lb, "lua_file")
   assert_not_nil(cstatic)
   assert_equals(ffi.C.CONFIG_ATTR_CLONED, bit.band(cstatic.attrs, ffi.C.CONFIG_ATTR_CLONED))

   assert_equals(0, exec_str("ubx.config_add(this, 'dyncfg', 'some description', 'int32_t')"))
   local dyncfg = ubx.config_get(lb, "dyncfg")
   assert_not_nil(dyncfg)
   assert_equals(0, bit.band(dyncfg.attrs, ffi.C.CONFIG_ATTR_CLONED))
end

function TestDynIF:TestPortAttrCloned()
   local pstatic = ubx.port_get(lb, "exec_str")
   assert_not_nil(pstatic)
   assert_equals(ffi.C.PORT_ATTR_CLONED, bit.band(pstatic.attrs, ffi.C.PORT_ATTR_CLONED))

   assert_equals(0, exec_str("ubx.inport_add(this, 'dynport', 'some description', 0, 'int32_t', 1)"))
   local dynport = ubx.port_get(lb, "dynport")
   assert_equals(0, bit.band(dynport.attrs, ffi.C.CONFIG_ATTR_CLONED))
end

os.exit( lu.LuaUnit.run() )
