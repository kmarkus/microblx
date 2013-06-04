
hexdump.so: hexdump.o $(INCLUDE_DIR)/u5c.h
	${CC} -shared -l:$(INCLUDE_DIR)/libu5c.so -o hexdump.so hexdump.o

hexdump.o: hexdump.c $(INCLUDE_DIR)/u5c.h
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) hexdump.c

clean:
	rm -f *.o *.so *~ core