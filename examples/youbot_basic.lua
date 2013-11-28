local bd = require("blockdiagram")

logger_report_conf = [[
{ { blockname='kin', portname="arm_out_msr_ee_pose", buff_len=3 },
  { blockname='kin', portname="arm_out_msr_ee_twist", buff_len=3, } }
]]

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
      { name="ybdrv", type="youbot/youbot_driver" },
      { name="kin", type="youbot_kin" },
      { name="ptrig1", type="std_triggers/ptrig" },
      { name="logger1", type="logging/file_logger" },
   },

   connections = {
      { src="ybdrv.arm1_state", tgt="kin.arm_in_jntstate", buffer_length=1 },
      { src="kin.arm_out_cmd_jnt_vel", tgt="ybdrv.arm1_cmd_vel", buffer_length=1 },
   },

   configurations = {
      { name="ybdrv", config = { ethernet_if="eth0" } },
      { name="logger1", config = { filename="kin.log", separator=",", report_conf = logger_report_conf, } }
   },
}
