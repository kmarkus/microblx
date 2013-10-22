# Configure this:

# create a symlink to the SOEM base directory.
SOEM_DIR=./soem/

ifeq (,$(wildcard $(SOEM_DIR)))
default: nosoem
else
default: youbot_driver.so
endif

# ifeq ("exists", $(shell [ -d $(SOEM_DIR) ] ) && echo "exists" )
#     @echo "SOEM found, building youbot_driver"
#     default=youbot_driver.so
# else
#     default=nosoem
# endif

.PHONY: nosoem

# SOEM 1.2.5
SOEM_LIBS=$(SOEM_DIR)/src/libsoem.a
SOEM_INC=-I$(SOEM_DIR)/src/

# SOEM 1.3
#SOEM_LIBS=$(SOEM_DIR)/lib/linux/libsoem.a $(SOEM_DIR)/lib/linux/liboshw.a $(SOEM_DIR)/lib/linux/libosal.a
#SOEM_INC=-I$(SOEM_DIR)/soem/ -I$(SOEM_DIR)/osal/ -I$(SOEM_DIR)/oshw/linux/

ROOT_DIR=$(CURDIR)/../..
include $(ROOT_DIR)/make.conf
UBX_DIR=$(ROOT_DIR)/src/

TYPES:=$(wildcard types/*.h)
HEXARRS:=$(TYPES:%=%.hexarr)

youbot_driver.so: youbot_driver.o $(UBX_DIR)/libubx.so
	${CC} $(CFLAGS_SHARED) -o youbot_driver.so youbot_driver.o $(SOEM_LIBS) $(UBX_DIR)/libubx.so -pthread -lrt

youbot_driver.o: youbot_driver.c youbot_driver.h $(UBX_DIR)/ubx.h $(UBX_DIR)/ubx_types.h $(UBX_DIR)/ubx.c $(HEXARRS)
	${CC} -fPIC -pthread -I$(UBX_DIR) $(SOEM_INC) -c $(CFLAGS) youbot_driver.c

nosoem:
	$(warning No 'soem' symlink found, not building youbot_driver)
	@:

clean:
	rm -f *.o *.so *~ core $(HEXARRS)
