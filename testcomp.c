
#include "u5c.h"

u5c_flowport ports[] = { 
	{ .name="vel", .dir=FP_DIR_OUT, .typename="kdl.twist" }, 
	{ .name="temp", .dir=FP_DIR_IN, .typename="int" },
	NULL,
};

u5c_component comp = {
	.type = "base_driver",
	.flowports = ports
};
