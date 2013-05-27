CFLAGS=-Wall -Werror

all: u5c.so random.so

u5c.so: u5c.o
	gcc -shared -o u5c.so u5c.o

u5c.o: u5c.c u5c.h
	gcc -fPIC -c ${CFLAGS} u5c.c

random.so: random.o
	gcc -shared -o random.so random.o

random.o: random.c u5c.h
	gcc -fPIC -c ${CFLAGS} random.c

clean:
	rm -f *.o *.so *~