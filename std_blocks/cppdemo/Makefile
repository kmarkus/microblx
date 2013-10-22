ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)

cppdemo.so: cppdemo.o $(INCLUDE_DIR)/libubx.so 
	${CPP} $(CFLAGS_SHARED) -o cppdemo.so cppdemo.o $(INCLUDE_DIR)/libubx.so

cppdemo.o: cppdemo.cpp $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c $(HEXARRS)
	${CPP} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) cppdemo.cpp

clean:
	rm -f *.o *.so *~ core $(HEXARRS)
