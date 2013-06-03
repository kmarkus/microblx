CC=clang
CFLAGS=-Wall -Werror -g -ggdb
SUBDIRS = std_blocks/random

.PHONY: subdirs $(SUBDIRS)
subdirs: libu5c.so $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

all: libu5c.so subdirs

libu5c.so: u5c.o
	${CC} -shared -o $@ $^

u5c.o: u5c.c u5c.h
	${CC} -fPIC -c ${CFLAGS} u5c.c

clean:
	rm -f *.o *.so *~ core