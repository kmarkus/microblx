ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)

%.h.hexarr: %.h
	../../tools/file2carr.lua $<

webif.so: webif.o mongoose.o $(INCLUDE_DIR)/libu5c.so 
	${CC} $(CFLAGS_SHARED)  -lpthread -l:$(INCLUDE_DIR)/libu5c.so -o webif.so webif.o mongoose.o

mongoose.o: mongoose.h mongoose.c
	${CC} -fPIC -c $(CFLAGS) mongoose.c

webif.o: webif.c $(INCLUDE_DIR)/u5c.h $(INCLUDE_DIR)/u5c_types.h $(INCLUDE_DIR)/u5c.c $(HEXARRS)
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) webif.c

clean:
	rm -f *.o *.so *~ core $(HEXARRS)