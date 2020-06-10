return block 
{
      name = "myblock",
      license = "MIT",
      meta_data = "",

      -- generate code for caching port poitners:
      port_cache = true,

      -- generate stubs and support for the following struct types:
      types = {
	 { name = "vector", class='struct' },
	 { name = "robot_data", class='struct' },
      },

      configurations = {
	 { name = "joint_limits", type_name = "double", min=0, max=6 },
	 { name = "info", type_name = "struct robot_data" },
      },

      ports = {
	 { name = "pos", in_type_name = "double", in_data_len = 5, doc="measured position [m]" },
	 { name = "ctrl_mode", in_type_name = "int32_t", out_type_name = "int32_t" },
      },

      operations = { start=true, stop=true, step=true }
}
