include ../make.conf
INCLUDE_DIR=../../src/

libu5c.so: u5c.o
	${CC} $(CFLAGS_SHARED) -o $@ $^

u5c_proto.h: u5c.c
	cproto u5c.c > $@

u5c.o: u5c.c u5c_proto.h u5c.h u5c_types.h uthash.h uthash_ffi.h
	${CC} -fPIC -c ${CFLAGS} u5c.c

clean:
	rm -f *.o *.so *~ core u5c_proto.h