ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)

%.h.hexarr: %.h
	../../tools/file2carr.lua $<

random.so: random.o $(INCLUDE_DIR)/libu5c.so 
	${CC} -shared -l:$(INCLUDE_DIR)/libu5c.so -o random.so random.o

random.o: random.c $(INCLUDE_DIR)/u5c.h $(INCLUDE_DIR)/u5c_types.h $(INCLUDE_DIR)/u5c.c $(HEXARRS)
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) random.c

clean:
	rm -f *.o *.so *~ core $(HEXARRS)