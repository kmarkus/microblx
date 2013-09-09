#!/usr/bin/luajit

ffi = require("ffi")
ubx = require("ubx")
time = require("time")
ubx_utils = require("ubx_utils")
ts = tostring

-- require"strict"
-- require"trace"

-- prog starts here.
ni=ubx.node_create("youbot")

-- load modules
ubx.load_module(ni, "std_types/stdtypes/stdtypes.so")
ubx.load_module(ni, "std_types/kdl/kdl_types.so")
ubx.load_module(ni, "std_blocks/webif/webif.so")
ubx.load_module(ni, "std_blocks/youbot_driver/youbot_driver.so")
ubx.load_module(ni, "std_blocks/ptrig/ptrig.so")
ubx.load_module(ni, "std_blocks/lfds_buffers/lfds_cyclic.so")
ubx.load_module(ni, "std_blocks/reporter/file_rep.so")

print("creating instance of 'webif/webif'")
webif1=ubx.block_create(ni, "webif/webif", "webif1", { port="8888" })

print("creating instance of 'youbot/youbot_driver'")
youbot1=ubx.block_create(ni, "youbot/youbot_driver", "youbot1", {ethernet_if="eth0" })

print("creating instance of 'std_triggers/ptrig'")
ptrig1=ubx.block_create(ni, "std_triggers/ptrig", "ptrig1", {trig_blocks={ { b=youbot1, num_steps=1, measure=0 } } } )

print("creating instance of 'reporter/file_rep'")

rep_conf=[[
{
   { blockname='youbot1', portname="base_motorinfo", buff_len=10, }
}
]]

file_rep1=ubx.block_create(ni, "reporter/file_rep", "file_rep1",
			   {filename=os.date("%Y%m%d_%H%M%S")..'_report.dat',
			    separator=',',
			    report_conf=rep_conf})

--- The following creates new ports that are automagically connected
--- to the specified peer port.
print("cloning base_control_mode port")
p_cmode = ubx.port_clone_conn(youbot1, "base_control_mode", 1, 1)
p_cmd_twist = ubx.port_clone_conn(youbot1, "base_cmd_twist", 1, 1)
p_cmd_vel = ubx.port_clone_conn(youbot1, "base_cmd_vel", 1, 1)
p_cmd_cur = ubx.port_clone_conn(youbot1, "base_cmd_cur", 1, 1)

cm_data=ubx.data_alloc(ni, "int32_t")

__time=ffi.new("struct ubx_timespec")
function gettime()
   ubx.clock_mono_gettime(__time)
   return {sec=tonumber(__time.sec), nsec=tonumber(__time.nsec)}
end

--- Configure the control mode.
-- @param mode control mode.
-- @return true if mode was set, false otherwise.
function set_control_mode(mode)
   ubx.data_set(cm_data, mode)
   ubx.port_write(p_cmode, cm_data)
   local res = ubx.port_read_timed(p_cmode, cm_data, 3)
   return ubx.data_tolua(cm_data)==mode
end

--- Return once the youbot is initialized or raise an error.
function youbot_initialized()
   local res=ubx.port_read_timed(p_cmode, cm_data, 5)
   return ubx.data_tolua(cm_data)==0 -- 0=MOTORSTOP
end



twist_data=ubx.data_alloc(ni, "struct kdl_twist")
null_twist_data=ubx.data_alloc(ni, "struct kdl_twist")

--- Move with a given twist.
-- @param twist table.
-- @param dur duration in seconds
function move_twist(twist_tab, dur)
   set_control_mode(2) -- VELOCITY
   ubx.data_set(twist_data, twist_tab)
   local ts_start=ffi.new("struct ubx_timespec")
   local ts_cur=ffi.new("struct ubx_timespec")

   ubx.clock_mono_gettime(ts_start)
   ubx.clock_mono_gettime(ts_cur)

   while ts_cur.sec - ts_start.sec < dur do
      ubx.port_write(p_cmd_twist, twist_data)
      ubx.clock_mono_gettime(ts_cur)
   end
   ubx.port_write(p_cmd_twist, null_twist_data)
end


vel_data=ubx.data_alloc(ni, "int32_t", 4)
null_vel_data=ubx.data_alloc(ni, "int32_t", 4)

--- Move each wheel with an individual RPM value.
-- @param table of size for with wheel velocity
-- @param dur time in seconds to apply velocity
function move_vel(vel_tab, dur)
   set_control_mode(2) -- VELOCITY
   ubx.data_set(vel_data, vel_tab)
   local dur = {sec=dur, nsec=0}
   local ts_start=gettime()
   local ts_cur=gettime()
   local diff = {sec=0,nsec=0}

   while true do
      diff.sec,diff.nsec=time.sub(ts_cur, ts_start)
      if time.cmp(diff, dur)==1 then break end
      ubx.port_write(p_cmd_vel, vel_data)
      ts_cur=gettime()
   end
   ubx.port_write(p_cmd_vel, null_vel_data)
end

cur_data=ubx.data_alloc(ni, "int32_t", 4)
null_cur_data=ubx.data_alloc(ni, "int32_t", 4)

--- Move each wheel with an individual RPM value.
-- @param table of size for with wheel velocity
-- @param dur time in seconds to apply velocity
function move_cur(cur_tab, dur)
   set_control_mode(6) -- CURRENT
   ubx.data_set(cur_data, cur_tab)

   local ts_start=ffi.new("struct ubx_timespec")
   local ts_cur=ffi.new("struct ubx_timespec")

   ubx.clock_mono_gettime(ts_start)
   ubx.clock_mono_gettime(ts_cur)

   while ts_cur.sec - ts_start.sec < dur do
      ubx.port_write(p_cmd_cur, cur_data)
      ubx.clock_mono_gettime(ts_cur)
   end
   ubx.port_write(p_cmd_cur, null_cur_data)
end

-- start and init webif and youbot
assert(ubx.block_init(ptrig1))
assert(ubx.block_init(file_rep1))
assert(ubx.block_init(webif1)==0)
assert(ubx.block_init(youbot1)==0)

assert(ubx.block_start(webif1)==0)
assert(ubx.block_start(file_rep1)==0)
assert(ubx.block_start(youbot1)==0)
assert(ubx.block_start(ptrig1)==0)


-- make sure youbot is running ok.
youbot_initialized()

twst={vel={x=0.05,y=0,z=0},rot={x=0,y=0,z=0.1}}
vel_tab={1,1,1,1}


-- ubx.node_cleanup(ni)
