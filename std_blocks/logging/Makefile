ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)
HEXARRS += file_logger.lua.hexarr

file_logger.so: file_logger.o $(INCLUDE_DIR)/libubx.so
	${CC} $(CFLAGS_SHARED) -o file_logger.so file_logger.o $(INCLUDE_DIR)/libubx.so -lluajit-5.1  -lpthread

file_logger.lua.hexarr: file_logger.lua
	../../tools/file2carr.lua file_logger.lua

file_logger.o: file_logger.c $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c $(HEXARRS)
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) file_logger.c

clean:
	rm -f *.o *.so *~ core $(HEXARRS)
