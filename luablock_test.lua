#!/usr/bin/luajit

local ffi = require("ffi")
local ubx = require "lua/ubx"
local ubx_utils = require("lua/ubx_utils")
local ts = tostring

code_str_len = 16*1024*1024

-- prog starts here.
ni=ubx.node_create("testnode")

ubx.load_module(ni, "std_types/stdtypes/stdtypes.so")
ubx.load_module(ni, "std_blocks/webif/webif.so")
ubx.load_module(ni, "std_blocks/luablock/luablock.so")
ubx.load_module(ni, "std_triggers/ptrig/ptrig.so")
ubx.load_module(ni, "std_blocks/lfds_buffers/lfds_cyclic.so")
ubx.ffi_load_types(ni)

print("creating instance of 'webif/webif'")
webif1=ubx.block_create(ni, "webif/webif", "webif1", { port="8888" })

print("creating instance of 'std_triggers/ptrig'")
ptrig1=ubx.block_create(ni, "std_triggers/ptrig", "ptrig1")

print("creating instance of 'lua/luablock'")
lb1=ubx.block_create(ni, "lua/luablock", "luablock1", 
		     { lua_file="/home/mk/prog/c/microblx/std_blocks/luablock/luablock.lua"})

fifo1=ubx.block_create(ni, "lfds_buffers/cyclic", "fifo1", {element_num=4, element_size=code_str_len})

print("running webif init", ubx.block_init(webif1))
print("running webif start", ubx.block_start(webif1))

p_exec_str=ubx.block_port_get(lb1, "exec_str")

print("running luablock init", ubx.block_init(lb1))
print("running fifo1 init", ubx.block_init(fifo1))
print("running fifo1 start", ubx.block_start(fifo1))

ubx.connect_one(p_exec_str, fifo1);

print("ni:", ni)
d1=ubx.data_alloc(ni, "char")
d2=ubx.data_alloc(ni, "int")
local cnt=0

for i=1,10000 do
   ubx.data_set(d1, "print('hi, code sent from outer space, cnt:"..tostring(cnt).."')", true)
   cnt=cnt+1
   ubx.interaction_write(fifo1, d1)
   lb1.step(lb1)
   print("read fifo1, len=", ubx.interaction_read(fifo1, d2), "val=", ubx.data_tostr(d2))
end

-- print("running ptrig1 unload", ubx.block_unload(ni, "ptrig1"))
-- print("running webif1 unload", ubx.block_unload(ni, "webif1"))
-- print("running luablock unload", ubx.block_unload(ni, "luablock1"))


-- ubx.unload_modules(ni)
-- ubx.ubx.ubx_node_cleanup(ni)
ubx.node_cleanup(ni)