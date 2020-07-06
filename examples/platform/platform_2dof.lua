return block
{
   name="platform_2dof",
   license="MIT",
   meta_data="",
   port_cache=true,

   configurations= {
      { name="joint_velocity_limits", type_name="double", min=2, max=2 },
      { name="initial_position", type_name="double", min=2, max=2 },
   },

   ports = {
      { name="pos", out_type_name="double", out_data_len=2, doc="measured position [m]" },
      { name="desired_vel", in_type_name="double", in_data_len=2, doc="desired velocity [m/s]" }
   },

   operations = { start=true, stop=true, step=true }
}
