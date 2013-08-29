ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)

testtypes.so: testtypes.o $(INCLUDE_DIR)/libubx.so
	${CC} -shared -o testtypes.so testtypes.o $(INCLUDE_DIR)/libubx.so

testtypes.o: testtypes.c $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c $(HEXARRS)
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) testtypes.c

clean:
	rm -f *.o *.so *~ core $(HEXARRS)
