CC=gcc
CFLAGS= -Wall -Wextra -Wpedantic -Werror -O3 -lpthread

all: test

lf_ll.o: lf_ll.c lf_ll.h

test: test.c lf_ll.o

.PHONY:
clean:
	rm -f test *.o
