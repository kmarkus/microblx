ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

.PHONY : liblfds611

liblfds611:
	make -C liblfds611

lfds_cyclic.so: lfds_cyclic.o $(INCLUDE_DIR)/libu5c.so liblfds611
	${CC} $(CFLAGS_SHARED) -l:$(INCLUDE_DIR)/libu5c.so -o lfds_cyclic.so lfds_cyclic.o liblfds611/bin/liblfds611.a

lfds_cyclic.o: lfds_cyclic.c $(INCLUDE_DIR)/u5c.h $(INCLUDE_DIR)/u5c_types.h $(INCLUDE_DIR)/u5c.c
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) lfds_cyclic.c

clean:
	rm -f *.o *.so *~ core