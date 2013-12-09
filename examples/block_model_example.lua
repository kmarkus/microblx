return block 
{
      name="myblock",
      meta_data="",
      port_cache=true,

      types = {
	 { name="vector", class='struct' }, -- Enum will follow once implemented in C
	 { name="robot_data", class='struct' },
      },

      configurations= {
	 { name="joint_limits", type_name="double", len=5 },
	 { name="info", type_name="struct robot_data" },
      },

      ports = {
	 { name="pos", in_type_name="double", in_data_len=5, doc="measured position [m]" },
	 { name="ctrl_mode", in_type_name="int32_t", out_type_name="int32_t" },
      },
      
      operations = { init=true, start=true, stop=true, step=true, cleanup=true }
}
