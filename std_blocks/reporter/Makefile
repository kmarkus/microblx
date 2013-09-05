ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)
HEXARRS += file_rep.lua.hexarr

file_rep.so: file_rep.o $(INCLUDE_DIR)/libubx.so 
	${CC} $(CFLAGS_SHARED) -o file_rep.so file_rep.o $(INCLUDE_DIR)/libubx.so -lluajit-5.1  -lpthread

file_rep.lua.hexarr: file_rep.lua
	../../tools/file2carr.lua file_rep.lua

file_rep.o: file_rep.c $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c $(HEXARRS)
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) file_rep.c

clean:
	rm -f *.o *.so *~ core $(HEXARRS)
