meta_data = [[
{
	doc="Cartesian space trajectory generator based on the OROCOS motion_control stack",
	license="GPLv2+",
}
]]

return block
{
      name="cart_trajgen",
      meta_data=meta_data,
      port_cache=true,
      cpp=true,

      types ={
	 -- { name="vector", class='struct' }, -- Enum will follow once implemented in C
	 -- { name="robot_data", class='struct' },
      },

      configurations= {
	 { name="max_vel", type_name="struct kdl_twist", doc="maximum velocity" },
	 { name="max_acc", type_name="struct kdl_twist", doc="maximum acceleration" },
      },

      ports = {
	 { name="des_pos", in_type_name="struct kdl_frame", doc="desired target position" },
	 { name="msr_pos", in_type_name="struct kdl_frame", doc="current measured position" },
	 { name="des_dur", in_type_name="double", doc="desired duration of trajectory [s] " },

	 { name="cmd_pos", out_type_name="struct kdl_frame", doc="next position increment" },
	 { name="cmd_vel", out_type_name="struct kdl_twist", doc="next velocity increment" },
	 { name="reached", out_type_name="int", doc="outputs 1 if the trajectory has been output entirely" },
	 { name="move_dur", out_type_name="double", doc="time since last move to command [s]" },
      },

      operations = { start=true, stop=true, step=true }
}
