local bd = require("blockdiagram")

return bd.system
{
   imports = { 
      "std_types/stdtypes/stdtypes.so",
      "std_types/kdl/kdl_types.so",
      "std_blocks/webif/webif.so",
      "std_blocks/youbot_driver/youbot_driver.so",
      "std_blocks/youbot_kindyn/youbot_kin.so",
      "std_blocks/ptrig/ptrig.so",
      "std_blocks/lfds_buffers/lfds_cyclic.so",
      "std_blocks/logging/file_logger.so",

   },
   
   blocks = {
      { name="yb1", type="youbot/youbot_driver" },
      { name="kin", type="youbot_kin" },
      { name="ptrig1", type="std_triggers/ptrig" },
   },
   
   connections = {
      -- { src="yb1.jnt_pos_msr", tgt="ctrl1.pos_msr", buffer_length=1 },
      -- { src="ctrl1.vel_des", tgt="yb1.jnt_vel_cmd", buffer_length=33 },
   },
   configurations = {
      { name="yb1", config={ ethernet_if="eth0" } },
      -- { name="ctrl1", config="controller.conf" },
   },
}
