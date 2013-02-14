CFLAGS=-Wall -Werror

random.so: random.o
	gcc -shared -o random.so random.o

random.o: random.c u5c.h
	gcc -fPIC -c ${CFLAGS} random.c

clean:
	rm -f *.o *.so *~