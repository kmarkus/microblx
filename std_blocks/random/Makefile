include ../../make.conf
INCLUDE_DIR=../../src/

random.so: random.o $(INCLUDE_DIR)/libu5c.so
	${CC} -shared -l:$(INCLUDE_DIR)/libu5c.so -o random.so random.o

random.o: random.c $(INCLUDE_DIR)/u5c.h $(INCLUDE_DIR)/u5c_types.h $(INCLUDE_DIR)/u5c.c
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) random.c

clean:
	rm -f *.o *.so *~ core