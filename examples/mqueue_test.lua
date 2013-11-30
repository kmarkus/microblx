---
--- Configure
---
-- [rnd] -> <mqueue> { OS } <mqueue> -> 
---


local bd = require("blockdiagram")


return bd.system
{
   imports = {
      "std_types/stdtypes/stdtypes.so",
      "std_blocks/webif/webif.so",
      "std_blocks/ptrig/ptrig.so",
      "std_blocks/random/random.so",
      "std_blocks/mqueue/mqueue.so",
      "std_blocks/logging/file_logger.so",

   },

   blocks = {
      { name="rnd", type="random/random" },
      { name="mq1", type="mqueue" },
      { name="ptrig1", type="std_triggers/ptrig" },
   },

   connections = {
      { src="rnd.rnd", tgt="mqueue" },
      -- { src="kin.arm_out_cmd_jnt_vel", tgt="ybdrv.arm1_cmd_vel", buffer_length=1 },
   },

   configurations = {
      { name="rnd", config = {min_max_config={min=1, max=128}}},
      { name="mq1", config = {mq_name="/mqtest", mq_maxmsg=8, mq_msgsize=32}},
   },
}
