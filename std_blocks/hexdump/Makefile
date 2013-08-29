ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

hexdump.so: hexdump.o $(INCLUDE_DIR)/libubx.so
	${CC} $(CFLAGS_SHARED) -o hexdump.so hexdump.o $(INCLUDE_DIR)/libubx.so

hexdump.o: hexdump.c $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) hexdump.c

clean:
	rm -f *.o *.so *~ core