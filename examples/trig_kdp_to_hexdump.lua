#!/usr/bin/env luajit

local ffi = require("ffi")
local ubx = require "ubx"
local ubx_utils = require("ubx_utils")
local ts = tostring

ni=ubx.node_create("testnode")

ubx.load_module(ni, "/usr/lib/microblx/stdtypes.so")
ubx.load_module(ni, "/usr/lib/microblx/testtypes.so")
ubx.load_module(ni, "/usr/lib/kb2ubx/dataprovider.so")
ubx.load_module(ni, "/usr/lib/microblx/hexdump.so")
ubx.load_module(ni, "/usr/lib/microblx/lfds_cyclic.so")
ubx.load_module(ni, "/usr/lib/microblx/webif.so")
--ubx.load_module(ni, "/usr/lib/microblx/logger.so")
ubx.load_module(ni, "/usr/lib/microblx/ptrig.so")
ubx.load_module(ni, "/usr/lib/kb2ubx/pegging.so")
ubx.load_module(ni, "/usr/lib/kb2ubx/meanvalue.so")
ubx.load_module(ni, "/usr/lib/kb2ubx/heatrelease.so")
ubx.load_module(ni, "/usr/lib/kb2ubx/pmax.so")
ubx.load_module(ni, "/usr/lib/kb2ubx/firstderivation.so")
ubx.load_module(ni, "/usr/lib/kb2ubx/imep.so")
ubx.load_module(ni, "/usr/lib/kb2ubx/noise.so")

ubx.ffi_load_types(ni)

print("creating instance of 'webif/webif'")
webif1=ubx.block_create(ni, "webif/webif", "webif1", { port="8888" })

print("creating instance of 'dataprovider/dataprovider'")
dataprovider1=ubx.block_create(ni, "dataprovider/dataprovider", "dataprovider1")

print("creating instance of 'pegging/pegging'")
pegging1=ubx.block_create(ni, "pegging/pegging", "pegging1")

print("creating instance of 'meanvalue/meanvalue'")
meanvalue1=ubx.block_create(ni, "meanvalue/meanvalue", "meanvalue1")

print("creating instance of 'heatrelease/heatrelease'")
heatrelease1=ubx.block_create(ni, "heatrelease/heatrelease", "heatrelease1")

print("creating instance of 'pmax/pmax'")
pmax1=ubx.block_create(ni, "pmax/pmax", "pmax1")

print("creating instance of 'firstderivation/firstderivation'")
firstderivation1=ubx.block_create(ni, "firstderivation/firstderivation", "firstderivation1")

print("creating instance of 'imep/imep'")
imep1=ubx.block_create(ni, "imep/imep", "imep1")

print("creating instance of 'noise/noise'")
noise1=ubx.block_create(ni, "noise/noise", "noise1")

print("creating instance of 'hexdump/hexdump'")
hexdump1=ubx.block_create(ni, "hexdump/hexdump", "hexdump1")

--print("creating instance of 'lfds_buffers/cyclic'")
--fifo1=ubx.block_create(ni, "lfds_buffers/cyclic", "fifo1", {element_num=4, element_size=4})

--print("creating instance of 'logging/file_logger'")

--logger_conf=[[
--{
--   { blockname='dataprovider1', portname="kdptr", buff_len=1, },
--   { blockname='ptrig1', portname="tstats", buff_len=3, }
--}
--]]

--file_log1=ubx.block_create(ni, "logging/file_logger", "file_log1",
--			   {filename=os.date("%Y%m%d_%H%M%S")..'_report.dat',
--			    separator=',',
--			    timestamp=1,
--			    report_conf=logger_conf})

print("creating instance of 'std_triggers/ptrig'")
ptrig1=ubx.block_create(ni, "std_triggers/ptrig", "ptrig1",
			{
			   period = {sec=1, usec=100000 },
			   sched_policy="SCHED_OTHER", sched_priority=0,
			   trig_blocks={ { b=dataprovider1, num_steps=1, measure=0 },
					 { b=pegging1, num_steps=1, measure=0 },
					 { b=meanvalue1, num_steps=1, measure=0 },
					 { b=heatrelease1, num_steps=1, measure=0 },
					 { b=pmax1, num_steps=1, measure=0 },
					 { b=firstderivation1, num_steps=1, measure=0 },
					 { b=imep1, num_steps=1, measure=0 },
					 { b=noise1, num_steps=1, measure=0 },
--					 { b=file_log1, num_steps=1, measure=0 }
			   } } )

-- ubx.ni_stat(ni)

print("running webif init", ubx.block_init(webif1))
print("running ptrig1 init", ubx.block_init(ptrig1))
print("running dataprovider1 init", ubx.block_init(dataprovider1))
print("running pegging1 init", ubx.block_init(pegging1))
print("running meanvalue1 init", ubx.block_init(meanvalue1))
print("running heatrelease1 init", ubx.block_init(heatrelease1))
print("running pmax1 init", ubx.block_init(pmax1))
print("running firstderivation1 init", ubx.block_init(firstderivation1))
print("running imep1 init", ubx.block_init(imep1))
print("running noise1 init", ubx.block_init(noise1))
print("running hexdump1 init", ubx.block_init(hexdump1))
--print("running fifo1 init", ubx.block_init(fifo1))
--print("running file_log1 init", ubx.block_init(file_log1))

print("running webif start", ubx.block_start(webif1))

provider_outport=ubx.port_get(dataprovider1, "cycle")
--pegging_inport=ubx.port_get(pegging1, "cycle")
--meanvalue_inport=ubx.port_get(meanvalue1, "cycle")
--heatrelease_inport=ubx.port_get(heatrelease1, "cycle")
--pmax_inport=ubx.port_get(pmax1, "cycle")
--firstderivation_inport=ubx.port_get(firstderivation1, "cycle")
--imep_inport=ubx.port_get(imep1, "cycle")
--noise_inport=ubx.port_get(noise1, "cycle")

print("connecting ports")
ubx.port_connect_out(provider_outport, hexdump1)
print("connecting dataprovider to pegging")
ubx.conn_lfds_cyclic(dataprovider1, "cycle", pegging1, "cycle_in", 1)
print("connecting pegging to meanvalue")
ubx.conn_lfds_cyclic(pegging1, "cycle_out", meanvalue1, "cycle", 1)
ubx.conn_lfds_cyclic(pegging1, "pegging_out", meanvalue1, "pegging_in", 1)
ubx.conn_lfds_cyclic(pegging1, "cycle_out", heatrelease1, "cycle", 1)
ubx.conn_lfds_cyclic(pegging1, "pegging_out", heatrelease1, "pegging_in", 1)
ubx.conn_lfds_cyclic(pegging1, "cycle_out", pmax1, "cycle", 1)
ubx.conn_lfds_cyclic(pegging1, "pegging_out", pmax1, "pegging_in", 1)
ubx.conn_lfds_cyclic(dataprovider1, "cycle", firstderivation1, "cycle_in", 1)
ubx.conn_lfds_cyclic(dataprovider1, "cycle", imep1, "cycle_in", 1)
ubx.conn_lfds_cyclic(dataprovider1, "cycle", noise1, "cycle_in", 1)
--ubx.port_connect_out(rand_port, fifo1)

--ubx.block_start(fifo1)
print("running dataprovider start", ubx.block_start(dataprovider1))
print("running pegging start", ubx.block_start(pegging1))
print("running meanvalue start", ubx.block_start(meanvalue1))
print("running heatrelease start", ubx.block_start(heatrelease1))
print("running pmax start", ubx.block_start(pmax1))
print("running firstderivation start", ubx.block_start(firstderivation1))
print("running imep start", ubx.block_start(imep1))
print("running noise start", ubx.block_start(noise1))
print("running hexdump start", ubx.block_start(hexdump1))

--print(utils.tab2str(ubx.block_totab(random1)))
print("--- demo app launched, browse to http://localhost:8888 and start ptrig1 block to start up")
io.read()

print("stopping and cleaning up blocks --------------------------------------------------------")
print("running ptrig1 unload", ubx.block_unload(ni, "ptrig1"))
print("running webif1 unload", ubx.block_unload(ni, "webif1"))
print("running dataprovider1 unload", ubx.block_unload(ni, "dataprovider1"))
print("running pegging1 unload", ubx.block_unload(ni, "pegging1"))
print("running meanvalue1 unload", ubx.block_unload(ni, "meanvalue1"))
print("running heatrelease1 unload", ubx.block_unload(ni, "heatrelease1"))
print("running pmax1 unload", ubx.block_unload(ni, "pmax1"))
print("running firstderivation1 unload", ubx.block_unload(ni, "firstderivation1"))
print("running imep1 unload", ubx.block_unload(ni, "imep1"))
print("running noise1 unload", ubx.block_unload(ni, "noise1"))
--print("running fifo1 unload", ubx.block_unload(ni, "fifo1"))
print("running hexdump1 unload", ubx.block_unload(ni, "hexdump1"))
--print("running file_log1 unload", ubx.block_unload(ni, "file_log1"))

-- ubx.ni_stat(ni)
-- l1=ubx.ubx_alloc_data(ni, "unsigned long", 1)
-- if l1~=nil then print_data(l1) end

ubx.unload_modules(ni)
-- ubx.ni_stat(ni)
os.exit(1)
