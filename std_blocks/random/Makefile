
random.so: random.o $(INCLUDE_DIR)/u5c.h
	${CC} -shared -l:$(INCLUDE_DIR)/libu5c.so -o random.so random.o

random.o: random.c $(INCLUDE_DIR)/u5c.h
	${CC} -fPIC -I$(INCLUDE_DIR) -c $(CFLAGS) random.c

clean:
	rm -f *.o *.so *~ core