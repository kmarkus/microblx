ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

default: mqueue.so mqueue_raw.so

mqueue.so: mqueue.o $(INCLUDE_DIR)/libubx.so
	${CC} $(CFLAGS_SHARED) -o mqueue.so mqueue.o $(INCLUDE_DIR)/libubx.so

mqueue.o: mqueue.c $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) mqueue.c

mqueue_raw.so: mqueue_raw.o $(INCLUDE_DIR)/libubx.so
	${CC} $(CFLAGS_SHARED) -o mqueue_raw.so mqueue_raw.o $(INCLUDE_DIR)/libubx.so

mqueue_raw.o: mqueue_raw.c $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) mqueue_raw.c

clean:
	rm -f *.o *.so *~ core
