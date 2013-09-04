ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)

fmpc.so: fmpc.o $(INCLUDE_DIR)/libubx.so 
	${CC} $(CFLAGS_SHARED) -o fmpc.so fmpc.o $(INCLUDE_DIR)/libubx.so

fmpc.o: fmpc.c $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c $(HEXARRS)
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) fmpc.c

clean:
	rm -f *.o *.so *~ core $(HEXARRS)
