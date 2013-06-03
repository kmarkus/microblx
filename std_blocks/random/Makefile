INCLUDE_DIR=../../

random.so: random.o ../../u5c.h
	${CC} -shared -l:../../libu5c.so -o random.so random.o

random.o: random.c ../../u5c.h
	${CC} -fPIC -I${INCLUDE_DIR} -c ${CFLAGS} random.c

clean:
	rm -f *.o *.so *~ core