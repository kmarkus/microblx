ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf

UBX_DIR=$(ROOT_DIR)/src/
EIGEN_DIR=/usr/include/eigen3/
YOUBOT_DRIVER_DIR=$(ROOT_DIR)/std_blocks/youbot_driver/types/
KDL_TYPES=$(ROOT_DIR)/std_types/kdl/types/

INCLUDES= -I$(UBX_DIR) -I$(EIGEN_DIR) -I$(YOUBOT_DRIVER_DIR) -I$(KDL_TYPES)

TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)

ifndef HAVE_KDL
default: no_youbot_kindyn
endif

.PHONY: no_youbot_kindyn

youbot_kin.so: youbot_kin.o $(UBX_DIR)/libubx.so
	${CPP} $(CFLAGS_SHARED) -o youbot_kin.so youbot_kin.o -lorocos-kdl $(UBX_DIR)/libubx.so

youbot_kin.o: youbot_kin.cpp $(UBX_DIR)/ubx.h $(UBX_DIR)/ubx_types.h $(UBX_DIR)/ubx.c $(HEXARRS)
	${CPP} -fPIC $(INCLUDES) -c $(CFLAGS) youbot_kin.cpp

clean:
	rm -f *.o *.so *~ core $(HEXARRS)

no_youbot_kindyn:
	$(warning Not building youbot_kindyn, set HAVE_KDL to enable build)
	@:
