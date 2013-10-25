ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

mqueue.so: mqueue.o $(INCLUDE_DIR)/libubx.so
	${CC} $(CFLAGS_SHARED) -o mqueue.so mqueue.o $(INCLUDE_DIR)/libubx.so

mqueue.o: mqueue.c $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) mqueue.c

clean:
	rm -f *.o *.so *~ core