#!/usr/bin/luajit

ffi = require("ffi")
ubx = require "ubx"
ubx_utils = require("ubx_utils")
ts = tostring

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

function set_control_mode(mode)
   ubx.data_set(cm_data, mode)
   ubx.interaction_write(i_control_mode, cm_data)
   ubx.cblock_step(youbot1)
   local res=ubx.interaction_read(i_control_mode, cm_data)
   if res<=0 then error("no response from set_control_mode") end
   return ubx.data_tolua(cm_data)==mode
end

function youbot_initialized()
   local res=ubx.interaction_read(i_control_mode, cm_data)
   if res<=0 then return false end
   return ubx.data_tolua(cm_data)==0 -- 0=MOTORSTOP
end

function send_twist(tw)
   ubx.interaction_write(i_twist, tw)
   ubx.cblock_step(youbot1)
end

twist_data=ubx.data_alloc(ni, "kdl/struct kdl_twist")
null_twist_data=ubx.data_alloc(ni, "kdl/struct kdl_twist")
function move_vel(twist_tab, dur)
   print("setting control_mode to vel (2)", set_control_mode(2)) -- VELOCITY
   ubx.data_set(twist_data, twist_tab)
   ts_start=ffi.new("struct ubx_timespec")
   ts_cur=ffi.new("struct ubx_timespec")

   ubx.clock_mono_gettime(ts_start)
   ubx.clock_mono_gettime(ts_cur)

   while ts_cur.sec - ts_start.sec < dur do
      send_twist(twist_data)
      ubx.clock_mono_gettime(ts_cur)
   end
   send_twist(null_twist_data)
end


--- Create an inversly connected port
-- @param bname block
-- @param pname name of port
-- @param buff_len1 desired buffer length (if in_out port: length of out->in buffer)
-- @param buff_len2 if in_out port: length of in->out buffer
function port_clone_conn(block, pname, buff_len1, buff_len2)
   local prot = ubx.port_get(b, pname)
   local p=ffi.new("ubx_port_t")
   local ts = ffi.new("struct ubx_timespec")

   ubx.clock_mono_gettime(ts)

   if ubx.clone_port_data(p, M.safe_tostr(prot.name)..'_inv', prot.meta_data,
			  prot.out_type, prot.out_type_len
			  prot.in_type, prot.in_type_len, state) ~= 0 then
      error("port_clone_conn: cloning port data failed")
   end

   if p.in_type and p.out_type then buff_len2 = buff_len2 or buff_len1 end

   local i_p_to_prot=nil
   if p.out_type then
      local iname = string.format("PCC->%s.%s_%x:%x",
				  M.safe_tostr(block.name), pname,
				  tonumber(ts.sec), tonumber(ts.nsec))

      i_p_to_prot = ubx.block_create(block.ni, "lfds_buffers/cyclic", iname,
				     { element_num=buff_len1,
				       element_size=tonumber(p.out_type.size * p.out_type_len) })

      ubx.block_init(i_p_to_prot)

      if ubx_ports_connect_uni(p, prot, i_p_to_prot)~=0 then
	 ubx.port_free_data(p)
	 error("failed to connect port"..M.safe_tostr(p.name))
      end
      ubx.block_start(i_p_to_prot)
   end

   local i_prot_to_p=nil
   if p.in_type then
      local iname = string.format("PCC<-%s.%s_%x:%x",
				  M.safe_tostr(block.name), pname,
				  tonumber(ts.sec), tonumber(ts.nsec))

      i_prot_to_p = ubx.block_create(block.ni, "lfds_buffers/cyclic", iname,
				     { element_num=buff_len1,
				       element_size=tonumber(p.in_type.size * p.in_type_len) })

      ubx.block_init(i_prot_to_p)

      if ubx.ports_connect_uni(prot, p, i_prot_to_p)~=0 then
	 -- TODO disconnect if connected above.
	 ubx.port_free_data(p)
	 error("failed to connect port"..M.safe_tostr(p.name))
      end
      ubx.block_start(i_in_out)
   end


   -- cleanup interaction and port once the reference is lost
   ffi.gc(p, function (p)
		print "cleaning up port PCC port "..M.safe_tostr(p.name)

		if i_p_to_prot then 
		   ubx.block_stop(i_p_to_prot) 
		   ubx_ports_disconnect_uni(p, prot, i_p_to_prot)
		   ubx.block_unload(b.ni, i_p_to_prot.name)
		end

		if i_prot_to_p then
		   ubx.block_stop(i_prot_to_p)
		   ubx_ports_disconnect_uni(prot, p, i_prot_to_p)
		   ubx.block_unload(b.ni, i_prot_to_p.name)
		end
	     end)
   return p
end

-- start webif
print("running webif init", ubx.block_init(webif1))
print("running webif start", ubx.block_start(webif1))

print("running youbot init", ubx.block_init(youbot1))
print("running youbot start", ubx.block_start(youbot1))

function start_vel(twist_tab, dur)
   print("setting control_mode to vel (2)", set_control_mode(2)) -- VELOCITY
   ubx.data_set(twist_data, twist_tab)
   ts_start=ffi.new("struct ubx_timespec")
   ts_cur=ffi.new("struct ubx_timespec")

   ubx.clock_mono_gettime(ts_start)
   ubx.clock_mono_gettime(ts_cur)

   while ts_cur.sec - ts_start.sec < dur do
      send_twist(twist_data)
      ubx.clock_mono_gettime(ts_cur)
   end

end

assert(ubx.block_start(ptrig1))

while true do
   if youbot_initialized() then
      break
   end
end


twst={vel={x=0.05,y=0,z=0},rot={x=0,y=0,z=0}}
-- move_vel(twst, 1)

-- start youbot and move it for five seconds in vel mode.

-- io.read()

-- ubx.node_cleanup(ni)
