ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

simple_fifo.so: simple_fifo.o $(INCLUDE_DIR)/libu5c.so
	${CC} $(CFLAGS_SHARED) -l:$(INCLUDE_DIR)/libu5c.so -o simple_fifo.so simple_fifo.o

simple_fifo.o: simple_fifo.c $(INCLUDE_DIR)/u5c.h $(INCLUDE_DIR)/u5c_types.h $(INCLUDE_DIR)/u5c.c
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) simple_fifo.c

clean:
	rm -f *.o *.so *~ core