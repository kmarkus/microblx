ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)

kdl_types.so: kdl_types.o $(INCLUDE_DIR)/libubx.so
	${CC} -shared -o kdl_types.so kdl_types.o $(INCLUDE_DIR)/libubx.so

kdl_types.o: kdl_types.c $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c $(HEXARRS)
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) kdl_types.c

clean:
	rm -f *.o *.so *~ core $(HEXARRS)
