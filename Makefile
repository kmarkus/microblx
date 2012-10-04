CFLAGS=-Wall -Werror

testcomp: testcomp.c u5c.h
	gcc -c ${CFLAGS} testcomp.c

clean:
	rm -f *.o *~