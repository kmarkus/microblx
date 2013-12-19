ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
INCLUDE_DIR=$(ROOT_DIR)/src/

default: lfds_cyclic.so lfds_cyclic_raw.so

lfds_cyclic.so: lfds_cyclic.o $(INCLUDE_DIR)/libubx.so liblfds611/bin/liblfds611.a
	${CC} $(CFLAGS_SHARED) -o lfds_cyclic.so lfds_cyclic.o liblfds611/bin/liblfds611.a $(INCLUDE_DIR)/libubx.so

lfds_cyclic.o: lfds_cyclic.c $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) lfds_cyclic.c

lfds_cyclic_raw.so: lfds_cyclic_raw.o $(INCLUDE_DIR)/libubx.so liblfds611/bin/liblfds611.a
	${CC} $(CFLAGS_SHARED) -o $@ $^ liblfds611/bin/liblfds611.a $(INCLUDE_DIR)/libubx.so

lfds_cyclic_raw.o: lfds_cyclic_raw.c $(INCLUDE_DIR)/ubx.h $(INCLUDE_DIR)/ubx_types.h $(INCLUDE_DIR)/ubx.c
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) lfds_cyclic_raw.c

liblfds611/bin/liblfds611.a:
	$(shell mkdir -p liblfds611/obj liblfds611/bin)
	make -C liblfds611


clean:
	rm -f *.o *.so *~ core
	make -C liblfds611 clean
