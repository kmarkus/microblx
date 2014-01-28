ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

# silence warnings due to unused rd/write helpers: remove once used!
CFLAGS:=$(CFLAGS) -Wno-unused-function

TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)

cart_trajgen.so: cart_trajgen.o $(INCLUDE_DIR)/libubx.so
	${CPP} $(CFLAGS_SHARED) -lorocos-kdl -o cart_trajgen.so cart_trajgen.o $(INCLUDE_DIR)/libubx.so

cart_trajgen.o: cart_trajgen.hpp cart_trajgen.cpp $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c $(HEXARRS)
	${CPP} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) cart_trajgen.cpp

clean:
	rm -f *.o *.so *~ core $(HEXARRS)
