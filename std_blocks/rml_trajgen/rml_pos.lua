meta_data = [[
{
	doc="velocity based trajectory generator using reflexxes library",
	license="LGPLv3",
}
]]

NUM_DOF=5

return block
{
      name="rml_pos",
      meta_data=meta_data,
      port_cache=true,
      cpp=true,

      types ={
	 -- { name="vector", class='struct' }, -- Enum will follow once implemented in C
	 -- { name="robot_data", class='struct' },
      },

      configurations= {
	 { name="max_vel", type_name="double", len=NUM_DOF, doc="maximum velocity" },
	 { name="max_acc", type_name="double", len=NUM_DOF, doc="maximum acceleration" },
	 { name="cycle_time", type_name="double", len=NUM_DOF, doc="cycle time [s]" },
      },

      ports = {
	 { name="msr_pos", in_type_name="double", in_data_len=NUM_DOF, doc="current measured position" },
	 { name="msr_vel", in_type_name="double", in_data_len=NUM_DOF, doc="current measured velocity" },

	 { name="des_pos", in_type_name="double", in_data_len=NUM_DOF, doc="desired target position" },
	 { name="des_vel", in_type_name="double", in_data_len=NUM_DOF, doc="desired target velocity" },

	 { name="cmd_pos", out_type_name="double", out_data_len=NUM_DOF, doc="new position (controller input)" },
	 { name="cmd_vel", out_type_name="double", out_data_len=NUM_DOF, doc="new velocity (controller input)" },
	 { name="cmd_acc", out_type_name="double", out_data_len=NUM_DOF, doc="new acceleration (controller input)" },

	 { name="reached", out_type_name="int", doc="the final state has been reached" },
      },

      operations = { start=true, stop=true, step=true }
}
