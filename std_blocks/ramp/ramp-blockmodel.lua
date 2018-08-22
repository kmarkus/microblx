return block 
{
      name="ramp",
      license="MIT",
      meta_data="",
      port_cache=true,

      types = {
      },

      configurations= {
	 { name="start", type_name="double", doc="ramp starting value (def 0)" },
	 { name="slope", type_name="double", doc="rate of change (def: 1)" },
      },
   
      ports = {
	 { name="out",
	   out_type_name="double",
	   out_data_len=1,
	   doc="ramp generator output" }
      },

      operations = { start=true, stop=true, step=true }
}
