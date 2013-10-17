#!/usr/local/bin/luajit

local ffi = require("ffi")
local ubx = require "ubx"
local ubx_utils = require("ubx_utils")
local ts = tostring

-- prog starts here.
ni=ubx.node_create("testnode")

ubx.load_module(ni, "std_types/stdtypes/stdtypes.so")
ubx.load_module(ni, "std_types/testtypes/testtypes.so")
ubx.load_module(ni, "std_blocks/random/random.so")
ubx.load_module(ni, "std_blocks/hexdump/hexdump.so")
ubx.load_module(ni, "std_blocks/lfds_buffers/lfds_cyclic.so")
ubx.load_module(ni, "std_blocks/webif/webif.so")
ubx.load_module(ni, "std_blocks/reporter/file_rep.so")
ubx.load_module(ni, "std_blocks/ptrig/ptrig.so")

ubx.ffi_load_types(ni)

print("creating instance of 'webif/webif'")
webif1=ubx.block_create(ni, "webif/webif", "webif1", { port="8888" })

print("creating instance of 'random/random'")
random1=ubx.block_create(ni, "random/random", "random1", {min_max_config={min=32, max=127}})

print("creating instance of 'hexdump/hexdump'")
hexdump1=ubx.block_create(ni, "hexdump/hexdump", "hexdump1")

print("creating instance of 'lfds_buffers/cyclic'")
fifo1=ubx.block_create(ni, "lfds_buffers/cyclic", "fifo1", {element_num=4, element_size=4})

print("creating instance of 'reporter/file_rep'")

rep_conf=[[
{
   { blockname='random1', portname="rnd", buff_len=1, },
   { blockname='fifo1', portname="overruns", buff_len=5, }
}
]]

file_rep1=ubx.block_create(ni, "reporter/file_rep", "file_rep1",
			   {filename=os.date("%Y%m%d_%H%M%S")..'_report.dat',
			    separator=',',
			    timestamp=1,
			    report_conf=rep_conf})

print("creating instance of 'std_triggers/ptrig'")
ptrig1=ubx.block_create(ni, "std_triggers/ptrig", "ptrig1",
			{
			   period = {sec=0, usec=100000 },
			   sched_policy="SCHED_OTHER", sched_priority=0,
			   trig_blocks={ { b=random1, num_steps=1, measure=0 },
					 { b=file_rep1, num_steps=1, measure=0 }
			   } } )

ubx.ni_stat(ni)

print("running webif init", ubx.block_init(webif1))
print("running ptrig1 init", ubx.block_init(ptrig1))
print("running random1 init", ubx.block_init(random1))
print("running hexdump1 init", ubx.block_init(hexdump1))
print("running fifo1 init", ubx.block_init(fifo1))

print("running webif start", ubx.block_start(webif1))

rand_port=ubx.port_get(random1, "rnd")

ubx.connect_one(rand_port, hexdump1)
ubx.connect_one(rand_port, fifo1)

ubx.block_start(fifo1)
ubx.block_start(random1)
ubx.block_start(hexdump1)

print(utils.tab2str(ubx.block_totab(random1)))
print("---")
--print(ubx.node_todot(ni))

-- local res, dat
-- --while true do
-- for i=1,8 do
--    ubx.cblock_step(random1)
--    --res, dat = interaction_read(fifo1)
--    --print("fifo1 read", res, data2str(dat))
--    -- os.execute("sleep 0.3")
-- end

-- for i=1,8 do
--    res, dat = ubx.interaction_read(ni, fifo1)
--    print("fifo1 read", res, ubx.data2str(dat))
--    -- os.execute("sleep 0.3")
-- end
-- --end
io.read()

print("stopping and cleaning up blocks --------------------------------------------------------")
print("running ptrig1 unload", ubx.block_unload(ni, "ptrig1"))
print("running webif1 unload", ubx.block_unload(ni, "webif1"))
print("running random1 unload", ubx.block_unload(ni, "random1"))
print("running fifo1 unload", ubx.block_unload(ni, "fifo1"))
print("running hexdump unload", ubx.block_unload(ni, "hexdump1"))

ubx.ni_stat(ni)

-- l1=ubx.ubx_alloc_data(ni, "unsigned long", 1)
-- if l1~=nil then print_data(l1) end

ubx.unload_modules(ni)
ubx.ni_stat(ni)
os.exit(1)
