#!/usr/bin/luajit

local ffi = require("ffi")
local u5c = require "lua/u5c"
local u5c_utils = require("lua/u5c_utils")
local ts = tostring

-- prog starts here.
ni=u5c.node_create("testnode")

u5c.load_module(ni, "std_types/stdtypes/stdtypes.so")
u5c.load_module(ni, "std_blocks/random/random.so")
u5c.load_module(ni, "std_blocks/hexdump/hexdump.so")
u5c.load_module(ni, "std_blocks/lfds_buffers/lfds_cyclic.so")
u5c.load_module(ni, "std_blocks/webif/webif.so")
u5c.ffi_load_types(ni)

print("creating instance of 'webif/webif'")
webif1=u5c.block_create(ni, "webif/webif", "webif1", { port="8888" })

print("creating instance of 'random/random'")
random1=u5c.block_create(ni, "random/random", "random1", {min_max_config={min=32, max=127}})

print("creating instance of 'hexdump/hexdump'")
hexdump1=u5c.block_create(ni, "hexdump/hexdump", "hexdump1")

print("creating instance of 'lfds_buffers/cyclic'")
fifo1=u5c.block_create(ni, "lfds_buffers/cyclic", "fifo1", {element_num=4, element_size=4})

u5c.ni_stat(ni)

--- Setup configuration
-- u5c.set_config(webif1, "port", "8081")
-- u5c.set_config(random1, "min_max_config", {min=77, max=88})

print("running webif init", u5c.block_init(ni, webif1))
print("running webif start", u5c.block_start(ni, webif1))

print("running random1 init", u5c.block_init(ni, random1))
print("running hexdump1 init", u5c.block_init(ni, hexdump1))
print("running fifo1 init", u5c.block_init(ni, fifo1))

rand_port=u5c.block_port_get(random1, "rnd")

u5c.connect_one(rand_port, hexdump1)
u5c.connect_one(rand_port, fifo1)

u5c.block_start(ni, fifo1)
u5c.block_start(ni, random1)
u5c.block_start(ni, hexdump1)


local res, dat
--while true do
for i=1,8 do
   u5c.cblock_step(random1)
   --res, dat = interaction_read(fifo1)
   --print("fifo1 read", res, data2str(dat))
   -- os.execute("sleep 0.3")
end

for i=1,8 do
   res, dat = u5c.interaction_read(ni, fifo1)
   print("fifo1 read", res, u5c.data2str(dat))
   -- os.execute("sleep 0.3")
end
--end
io.read()

print("stopping and cleaning up blocks --------------------------------------------------------")

print("running webif1 unload", u5c.block_unload(ni, "webif1"))
print("running random1 unload", u5c.block_unload(ni, "random1"))
print("running fifo1 unload", u5c.block_unload(ni, "fifo1"))
print("running hexdump unload", u5c.block_unload(ni, "hexdump1"))

u5c.ni_stat(ni)

-- l1=u5c.u5c_alloc_data(ni, "unsigned long", 1)
-- if l1~=nil then print_data(l1) end

u5c.unload_modules()
u5c.ni_stat(ni)
os.exit(1)
