#!/usr/bin/luajit

local ffi = require("ffi")
local u5c = require "lua/u5c"
local u5c_utils = require("lua/u5c_utils")
local ts = tostring

-- prog starts here.
ni=u5c.init_node()

u5c.load_module(ni, "std_types/stdtypes/stdtypes.so")
u5c.load_module(ni, "std_blocks/random/random.so")
u5c.load_module(ni, "std_blocks/hexdump/hexdump.so")
u5c.load_module(ni, "std_blocks/lfds_buffers/lfds_cyclic.so")
u5c.load_module(ni, "std_blocks/webif/webif.so")

print("creating instance of 'webif'")
webif1=u5c.cblock_create(ni, "webif", "webif1")

print("creating instance of 'random'")
random1=u5c.cblock_create(ni, "random", "random1")

u5c.ffi_load_types(ni)

print("creating instance of 'hexdump'")
hexdump1=u5c.iblock_create(ni, "hexdump", "hexdump1")

print("creating instance of 'fifo'")
fifo1=u5c.iblock_create(ni, "lfds_cyclic", "fifo1")

u5c.ni_stat(ni)

print("running webif init", u5c.block_init(ni, webif1))
print("running webif start", u5c.block_start(ni, webif1))

print("running random1 init", u5c.block_init(ni, random1))
print("running hexdump1 init", u5c.block_init(ni, hexdump1))
print("running fifo1 init", u5c.block_init(ni, fifo1))

rand_port=u5c.block_port_get(random1, "rnd")

u5c.connect_one(rand_port, hexdump1)
u5c.connect_one(rand_port, fifo1)

local res, dat
for i=1,8 do
   random1.step(random1)
   --res, dat = interaction_read(fifo1)
   --print("fifo1 read", res, data2str(dat))
   -- os.execute("sleep 0.1")
end

for i=1,8 do
   res, dat = u5c.interaction_read(ni, fifo1)
   print("fifo1 read", res, u5c.data2str(dat))
   -- os.execute("sleep 0.1")
end

io.read()

print("cleaning up blocks --------------------------------------------------------")
print("running webif stop", u5c.block_stop(ni, webif1))
print("webif1 cleanup", u5c.block_rm(ni, ffi.C.BLOCK_TYPE_COMPUTATION, "webif1"))

print("fifo1 cleanup", u5c.block_cleanup(ni, fifo1))
print("random1 cleanup", u5c.block_rm(ni, ffi.C.BLOCK_TYPE_COMPUTATION, "random1"))
print("hexdump1 cleanup", u5c.block_rm(ni, ffi.C.BLOCK_TYPE_INTERACTION, "hexdump1"))
print("fifo1 cleanup", u5c.block_rm(ni, ffi.C.BLOCK_TYPE_INTERACTION, "fifo1"))


print(tostring(u5c.u5c.u5c_num_cblocks(ni)).." blocks loaded")

u5c.ni_stat(ni)

-- l1=u5c.u5c_alloc_data(ni, "unsigned long", 1)
-- if l1~=nil then print_data(l1) end

u5c.unload_modules()
u5c.ni_stat(ni)
os.exit(1)
