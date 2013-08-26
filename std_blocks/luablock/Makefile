ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)

luablock.so: luablock.o $(INCLUDE_DIR)/libubx.so 
	${CC} $(CFLAGS_SHARED) -lluajit-5.1  -lpthread -l:$(INCLUDE_DIR)/libubx.so -o luablock.so luablock.o

luablock.o: luablock.c $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c $(HEXARRS)
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) luablock.c

clean:
	rm -f *.o *.so *~ core $(HEXARRS)
