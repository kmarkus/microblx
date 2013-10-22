ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf

UBX_DIR=$(ROOT_DIR)/src/
EIGEN_DIR=/usr/include/eigen3/

INCLUDES= -I$(UBX_DIR) -I$(EIGEN_DIR)

TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)

youbot_kin.so: youbot_kin.o $(UBX_DIR)/libubx.so
	${CPP} $(CFLAGS_SHARED) -o youbot_kin.so -lorocos-kdl youbot_kin.o $(UBX_DIR)/libubx.so

youbot_kin.o: youbot_kin.cpp $(UBX_DIR)/ubx.h $(UBX_DIR)/ubx_types.h $(UBX_DIR)/ubx.c $(HEXARRS)
	${CPP} -fPIC $(INCLUDES) -c $(CFLAGS) youbot_kin.cpp

clean:
	rm -f *.o *.so *~ core $(HEXARRS)
