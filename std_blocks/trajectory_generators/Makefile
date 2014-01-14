ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

REFLEXXES_LIB=$(CURDIR)/reflexxes/Linux/x64/debug/lib/static/libReflexxesTypeII.a

TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)

rml_vel.so: rml_vel.o $(INCLUDE_DIR)/libubx.so
	${CPP} $(CFLAGS_SHARED) -o rml_vel.so rml_vel.o $(INCLUDE_DIR)/libubx.so $(REFLEXXES_LIB)

rml_vel.o: rml_vel.hpp rml_vel.cpp $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c $(HEXARRS)
	${CPP} -fPIC -I$(INCLUDE_DIR) -Ireflexxes/include -c $(CFLAGS) rml_vel.cpp

clean:
	rm -f *.o *.so *~ core $(HEXARRS)
