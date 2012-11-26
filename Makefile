CFLAGS=-Wall -Werror

random: random.c u5c.h
	gcc -c ${CFLAGS} random.c

clean:
	rm -f *.o *~