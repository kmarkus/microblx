ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

# silence warnings due to unused rd/write helpers: remove once used!
CFLAGS:=$(CFLAGS) -Wno-unused-function

TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)

rml_vel.so: rml_vel.o $(INCLUDE_DIR)/libubx.so
	${CC} $(CFLAGS_SHARED) -o rml_vel.so rml_vel.o $(INCLUDE_DIR)/libubx.so

rml_vel.o: rml_vel.h rml_vel.c $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c $(HEXARRS)
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) rml_vel.c

clean:
	rm -f *.o *.so *~ core $(HEXARRS)
