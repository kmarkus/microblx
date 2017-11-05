ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

# compile types to hexarrays
TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)

%.h.hexarr: %.h
	../../tools/file2carr.lua $<

trig.so: trig.o $(INCLUDE_DIR)/libubx.so
	${CC} $(CFLAGS_SHARED) -o trig.so trig.o -pthread $(INCLUDE_DIR)/libubx.so

trig.o: trig.c $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c $(HEXARRS)
	${CC} -fPIC -pthread -I$(INCLUDE_DIR) -c $(CFLAGS) trig.c

clean:
	rm -f *.o *.so *~ core $(HEXARRS)
