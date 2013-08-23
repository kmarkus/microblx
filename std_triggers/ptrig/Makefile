ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

# compile types to hexarrays
TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)

%.h.hexarr: %.h
	../../tools/file2carr.lua $<

ptrig.so: ptrig.o $(INCLUDE_DIR)/libubx.so
	${CC} $(CFLAGS_SHARED) -pthread -l:$(INCLUDE_DIR)/libubx.so -o ptrig.so ptrig.o

ptrig.o: ptrig.c $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c $(HEXARRS)
	${CC} -fPIC -pthread -I$(INCLUDE_DIR) -c $(CFLAGS) ptrig.c

clean:
	rm -f *.o *.so *~ core $(HEXARRS)