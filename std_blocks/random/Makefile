ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)

random.so: random.o $(INCLUDE_DIR)/libubx.so 
	${CC} $(CFLAGS_SHARED) -o random.so random.o $(INCLUDE_DIR)/libubx.so

random.o: random.c $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c $(HEXARRS)
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) random.c

clean:
	rm -f *.o *.so *~ core $(HEXARRS)
