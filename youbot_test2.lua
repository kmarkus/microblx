#!/usr/bin/luajit

ffi = require("ffi")
ubx = require "ubx"
ubx_utils = require("ubx_utils")
ts = tostring
-- require"strict"
-- require"trace"

--- Create an inversly connected port
-- @param bname block
-- @param pname name of port
-- @param buff_len1 desired buffer length (if in_out port: length of out->in buffer)
-- @param buff_len2 if in_out port: length of in->out buffer
function port_clone_conn(block, pname, buff_len1, buff_len2)
   local prot = ubx.port_get(block, pname)
   local p=ffi.new("ubx_port_t")
   local ts = ffi.new("struct ubx_timespec")

   -- print("port_clone_conn, block=", ubx.safe_tostr(block.name), ", port=", pname)

   ubx.clock_mono_gettime(ts)

   if ubx.clone_port_data(p, ubx.safe_tostr(prot.name)..'_inv', prot.meta_data,
			  prot.out_type, prot.out_data_len,
			  prot.in_type, prot.in_data_len, 0) ~= 0 then
      error("port_clone_conn: cloning port data failed")
   end

   buff_len1=buff_len1 or 1
   if p.in_type and p.out_type then buff_len2 = buff_len2 or buff_len1 end

   -- New port is an out-port?
   local i_p_to_prot=nil
   if p.out_type~=nil then
      local iname = string.format("PCC->%s.%s_%x:%x",
				  ubx.safe_tostr(block.name), pname,
				  tonumber(ts.sec), tonumber(ts.nsec))

      -- print("creating interaction", iname, buff_len1, tonumber(p.out_type.size * p.out_data_len))

      i_p_to_prot = ubx.block_create(block.ni, "lfds_buffers/cyclic", iname,
				     { element_num=buff_len1,
				       element_size=tonumber(p.out_type.size * p.out_data_len) })

      ubx.block_init(i_p_to_prot)

      if ubx.ports_connect_uni(p, prot, i_p_to_prot)~=0 then
	 ubx.port_free_data(p)
	 error("failed to connect port"..ubx.safe_tostr(p.name))
      end
      ubx.block_start(i_p_to_prot)
   end

   -- New port is (also) an in-port?
   local i_prot_to_p=nil
   if p.in_type ~= nil then
      local iname = string.format("PCC<-%s.%s_%x:%x",
				  ubx.safe_tostr(block.name), pname,
				  tonumber(ts.sec), tonumber(ts.nsec))

      -- print("creating interaction", iname, buff_len2, tonumber(p.in_type.size * p.in_data_len))

      i_prot_to_p = ubx.block_create(block.ni, "lfds_buffers/cyclic", iname,
				     { element_num=buff_len2,
				       element_size=tonumber(p.in_type.size * p.in_data_len) })

      ubx.block_init(i_prot_to_p)

      if ubx.ports_connect_uni(prot, p, i_prot_to_p)~=0 then
	 -- TODO disconnect if connected above.
	 ubx.port_free_data(p)
	 error("failed to connect port"..ubx.safe_tostr(p.name))
      end
      ubx.block_start(i_prot_to_p)
   end


   -- cleanup interaction and port once the reference is lost
   ffi.gc(p, function (p)
		print("cleaning up port PCC port "..ubx.safe_tostr(p.name))

		if i_p_to_prot then 
		   ubx.block_stop(i_p_to_prot) 
		   ubx.ports_disconnect_uni(p, prot, i_p_to_prot)
		   ubx.block_unload(block.ni, i_p_to_prot.name)
		end

		if i_prot_to_p then
		   ubx.block_stop(i_prot_to_p)
		   ubx.ports_disconnect_uni(prot, p, i_prot_to_p)
		   ubx.block_unload(block.ni, i_prot_to_p.name)
		end
		ubx.port_free_data(p)
	     end)
   return p
end


function port_read_timed(p, data, sec)
   local res
   local ts_start=ffi.new("struct ubx_timespec")
   local ts_cur=ffi.new("struct ubx_timespec")

   ubx.clock_mono_gettime(ts_start)
   ubx.clock_mono_gettime(ts_cur)

   while ts_cur.sec - ts_start.sec < sec do
      res=ubx.port_read(p, data)
      if res>0 then return res end
      ubx.clock_mono_gettime(ts_cur)
   end
   error("port_read_timed: timeout after reading "..ubx.safe_tostr(p.name).." for "..tostring(sec).." seconds")
end

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
ptrig1=ubx.block_create(ni, "std_triggers/ptrig", "ptrig1", {trig_blocks={ { b=youbot1, num_steps=1, measure=0 } } } )

assert(ubx.block_init(ptrig1))

--- The following creates new ports that are automagically connected
--- to the specified peer port.
print("cloning base_control_mode port")
p_cmode = port_clone_conn(youbot1, "base_control_mode", 1, 1)
p_cmd_twist = port_clone_conn(youbot1, "base_cmd_twist", 1, 1)
p_cmd_vel = port_clone_conn(youbot1, "base_cmd_vel", 1, 1)
p_cmd_cur = port_clone_conn(youbot1, "base_cmd_cur", 1, 1)

cm_data=ubx.data_alloc(ni, "int32_t")

--- Configure the control mode.
-- @param mode control mode.
-- @return true if mode was set, false otherwise.
function set_control_mode(mode)
   ubx.data_set(cm_data, mode)
   ubx.port_write(p_cmode, cm_data)
   local res = port_read_timed(p_cmode, cm_data, 3)
   return ubx.data_tolua(cm_data)==mode
end

--- Return once the youbot is initialized or raise an error.
function youbot_initialized()
   local res=port_read_timed(p_cmode, cm_data, 5)
   return ubx.data_tolua(cm_data)==0 -- 0=MOTORSTOP
end



twist_data=ubx.data_alloc(ni, "kdl/struct kdl_twist")
null_twist_data=ubx.data_alloc(ni, "kdl/struct kdl_twist")

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
   local ts_start=ffi.new("struct ubx_timespec")
   local ts_cur=ffi.new("struct ubx_timespec")

   ubx.clock_mono_gettime(ts_start)
   ubx.clock_mono_gettime(ts_cur)

   while ts_cur.sec - ts_start.sec < dur do
      ubx.port_write(p_cmd_vel, vel_data)
      ubx.clock_mono_gettime(ts_cur)
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
assert(ubx.block_init(webif1)==0)
assert(ubx.block_start(webif1)==0)
assert(ubx.block_init(youbot1)==0)
assert(ubx.block_start(youbot1)==0)

-- make sure youbot is running ok.
assert(ubx.block_start(ptrig1))
youbot_initialized()

twst={vel={x=0.05,y=0,z=0},rot={x=0,y=0,z=0.1}}
vel_tab={1,1,1,1}


-- ubx.node_cleanup(ni)
