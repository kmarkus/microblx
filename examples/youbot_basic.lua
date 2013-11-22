local bd = require("blockdiagram")

return bd.system
{
   imports = { "youbot_driver", "controller" },
   
   blocks = {
      { name="yb1", type="youbot_driver" },
      { name="ctrl1", type="controller" },
   },
   
   connections = {
      { src="yb1.jnt_pos_msr", tgt="ctrl1.pos_msr", buffer_length=1 },
      { src="ctrl1.vel_des", tgt="yb1.jnt_vel_cmd", buffer_length=1 },
   },
   configurations = {
      { name="yb1", config={ eth_if="eth0" } },
      { name="ctrl1", config="controller.conf" },
   },
}
