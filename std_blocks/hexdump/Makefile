ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

hexdump.so: hexdump.o $(INCLUDE_DIR)/libu5c.so
	${CC} $(CFLAGS_SHARED) -l:$(INCLUDE_DIR)/libu5c.so -o hexdump.so hexdump.o

hexdump.o: hexdump.c $(INCLUDE_DIR)/u5c.h $(INCLUDE_DIR)/u5c_types.h $(INCLUDE_DIR)/u5c.c
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) hexdump.c

clean:
	rm -f *.o *.so *~ core