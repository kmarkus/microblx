ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

stdtypes.so: stdtypes.o $(INCLUDE_DIR)/libu5c.so
	${CC} -shared -l:$(INCLUDE_DIR)/libu5c.so -o stdtypes.so stdtypes.o

stdtypes.o: stdtypes.c $(INCLUDE_DIR)/u5c.h $(INCLUDE_DIR)/u5c_types.h $(INCLUDE_DIR)/u5c.c
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) stdtypes.c

clean:
	rm -f *.o *.so *~ core