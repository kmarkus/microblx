ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)
HEXARRS += webif.lua.hexarr

webif.so: webif.o mongoose.o $(INCLUDE_DIR)/libubx.so 
	${CC} $(CFLAGS_SHARED) -o webif.so webif.o mongoose.o -lluajit-5.1 -lpthread $(INCLUDE_DIR)/libubx.so

mongoose.o: mongoose.h mongoose.c
	${CC} -fPIC -c $(CFLAGS) mongoose.c

webif.lua.hexarr: webif.lua
	../../tools/file2carr.lua webif.lua

webif.o: webif.c $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c $(HEXARRS) webif.lua.hexarr
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) webif.c

clean:
	rm -f *.o *.so *~ core $(HEXARRS)
