ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

simple_fifo.so: simple_fifo.o $(INCLUDE_DIR)/libubx.so
	${CC} $(CFLAGS_SHARED) -o simple_fifo.so simple_fifo.o $(INCLUDE_DIR)/libubx.so

simple_fifo.o: simple_fifo.c $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) simple_fifo.c

clean:
	rm -f *.o *.so *~ core