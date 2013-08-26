# Configure this:
SOEM_DIR=/home/mk/src/tarballs/SOEM1.3.0

ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
UBX_DIR=$(ROOT_DIR)/src/

INCLUDES=-I$(SOEM_DIR)/soem/ -I$(SOEM_DIR)/osal/ -I$(SOEM_DIR)/oshw/linux/

TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)

youbot_driver.so: youbot_driver.o $(UBX_DIR)/libubx.so 
	${CC} $(CFLAGS_SHARED) -l:$(UBX_DIR)/libubx.so -l:$(SOEM_DIR)/lib/linux/libsoem.a -o youbot_driver.so youbot_driver.o

youbot_driver.o: youbot_driver.c youbot_driver.h $(UBX_DIR)/ubx.h $(UBX_DIR)/ubx_types.h $(UBX_DIR)/ubx.c $(HEXARRS)
	${CC} -fPIC -I$(UBX_DIR) $(INCLUDES) -c $(CFLAGS) youbot_driver.c

clean:
	rm -f *.o *.so *~ core $(HEXARRS)
