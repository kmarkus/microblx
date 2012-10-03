
#include "u5c.h"
/* #declare_type( */

u5c_port ports[] = { 
	{ .name="vel", .dir=FP_DIR_OUT, .typename="kdl_twist" }, 
	{ .name="temp", .dir=FP_DIR_IN, .typename="int" },
	NULL,
};

u5c_component comp = {
	.type = "base_driver",
	.ports = ports
};
