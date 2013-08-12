ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
CC=clang++
INCLUDE_DIR=$(ROOT_DIR)/src/

TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)

%.h.hexarr: %.h
	../../tools/file2carr.lua $<

cppdemo.so: cppdemo.o $(INCLUDE_DIR)/libubx.so 
	${CC} $(CFLAGS_SHARED) -l:$(INCLUDE_DIR)/libubx.so -o cppdemo.so cppdemo.o

cppdemo.o: cppdemo.c $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c $(HEXARRS)
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) cppdemo.c

clean:
	rm -f *.o *.so *~ core $(HEXARRS)