#!/usr/bin/luajit

local ffi = require("ffi")
local ubx = require "ubx"
local ubx_utils = require("ubx_utils")
local ts = tostring

-- prog starts here.
ni=ubx.node_create("youbot")

-- load modules
ubx.load_module(ni, "std_types/stdtypes/stdtypes.so")
ubx.load_module(ni, "std_types/kdl/kdl_types.so")
ubx.load_module(ni, "std_blocks/webif/webif.so")
ubx.load_module(ni, "std_blocks/youbot_driver/youbot_driver.so")
ubx.load_module(ni, "std_triggers/ptrig/ptrig.so")
ubx.load_module(ni, "std_blocks/lfds_buffers/lfds_cyclic.so")

print("creating instance of 'webif/webif'")
webif1=ubx.block_create(ni, "webif/webif", "webif1", { port="8888" })

print("creating instance of 'youbot/youbot_driver'")
youbot1=ubx.block_create(ni, "youbot/youbot_driver", "youbot1", {ethernet_if="eth0" })

print("creating instance of 'std_triggers/ptrig'")
ptrig1=ubx.block_create(ni, "std_triggers/ptrig", "ptrig1",
			{trig_blocks={ { b=youbot1, num_steps=1, measure=0 } } } )


i_twist=ubx.block_create(ni, "lfds_buffers/cyclic", "i_twist",
			 { element_num=4, element_size=ubx.type_size(ni, "kdl/struct kdl_twist") })

i_control_mode=ubx.block_create(ni, "lfds_buffers/cyclic", "i_control_mode",
				{ element_num=2, element_size=ubx.type_size(ni, "int32_t") })

assert(ubx.block_init(i_twist))
assert(ubx.block_start(i_twist))
assert(ubx.block_init(i_control_mode))
assert(ubx.block_start(i_control_mode))

assert(ubx.block_init(ptrig1))

ubx.connect_one(ubx.port_get(youbot1, "base_cmd_twist"), i_twist)
ubx.connect_one(ubx.port_get(youbot1, "base_control_mode"), i_control_mode)

cm_data=ubx.data_alloc(ni, "int32_t")
twist_data=ubx.data_alloc(ni, "kdl/struct kdl_twist")

function set_control_mode(mode)
   ubx.data_set(cm_data, mode)
   ubx.interaction_write(i_control_mode, cm_data)
   ubx.cblock_step(youbot1)
   local res=ubx.interaction_read(i_control_mode, cm_data)
   if res<=0 then error("no response from set_control_mode") end
   return ubx.data_tolua(cd_data)
end

-- start webif
print("running webif init", ubx.block_init(webif1))
print("running webif start", ubx.block_start(webif1))

print("running youbot init", ubx.block_init(youbot1))
print("running youbot start", ubx.block_start(youbot1))

assert(ubx.block_start(ptrig1))


-- start youbot and move it for five seconds in vel mode.

io.read()

ubx.node_cleanup(ni)


