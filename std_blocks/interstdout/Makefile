
stdout.so: stdout.o $(INCLUDE_DIR)/u5c.h
	${CC} -shared -l:$(INCLUDE_DIR)/libu5c.so -o stdout.so stdout.o

stdout.o: stdout.c $(INCLUDE_DIR)/u5c.h
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) stdout.c

clean:
	rm -f *.o *.so *~ core