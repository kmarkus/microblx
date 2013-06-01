CC=clang
CFLAGS=-Wall -Werror -g -ggdb

all: libu5c.so random.so

libu5c.so: u5c.o
	${CC} -shared -o libu5c.so u5c.o

u5c.o: u5c.c u5c.h
	${CC} -fPIC -c ${CFLAGS} u5c.c

random.so: random.o libu5c.so
	${CC} -shared -l:./libu5c.so -o random.so random.o

random.o: random.c libu5c.so
	${CC} -fPIC -c ${CFLAGS} random.c

clean:
	rm -f *.o *.so *~ core