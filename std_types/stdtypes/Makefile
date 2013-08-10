ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

stdtypes.so: stdtypes.o $(INCLUDE_DIR)/libubx.so
	${CC} -shared -l:$(INCLUDE_DIR)/libubx.so -o stdtypes.so stdtypes.o

stdtypes.o: stdtypes.c $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) stdtypes.c

clean:
	rm -f *.o *.so *~ core