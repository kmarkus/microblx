ROOT_DIR=$(CURDIR)/..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src


c-only: c-only.c $(INCLUDE_DIR)/ubx.h
	${CC} -ldl -I$(INCLUDE_DIR) $(CFLAGS) $^ -o $@ $(INCLUDE_DIR)/libubx.so

clean:
	rm -f c-only *.o *~ core
