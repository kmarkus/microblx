ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

buffer.so: buffer.o $(INCLUDE_DIR)/libu5c.so
	${CC} $(CFLAGS_SHARED) -l:$(INCLUDE_DIR)/libu5c.so -o buffer.so buffer.o

buffer.o: buffer.c $(INCLUDE_DIR)/u5c.h $(INCLUDE_DIR)/u5c_types.h $(INCLUDE_DIR)/u5c.c
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) buffer.c

clean:
	rm -f *.o *.so *~ core