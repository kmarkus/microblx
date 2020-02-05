return block
{
      name="pid",
      license="BSD-3-Clause",
      meta_data="PID controller block",
      port_cache=true,

      types = {
      },

      configurations= {
	 { name="Kp", type_name="double", doc="P-gain (def: 0)" },
	 { name="Ki", type_name="double", doc="I-gain (def: 0)" },
	 { name="Kd", type_name="double", doc="D-gain (def: 0)" },
	 { name="data_len", type_name="unsigned int", doc="length of signal array (def: 1)" },
      },

      ports = {
	 { name="msr", in_type_name="double", in_data_len=1, doc="measured input signal" },
	 { name="des", in_type_name="double", in_data_len=1, doc="desired input signal" },
	 { name="out", out_type_name="double", out_data_len=1, doc="controller output" },
      },

      operations = { start=true, stop=true, step=true }
}
