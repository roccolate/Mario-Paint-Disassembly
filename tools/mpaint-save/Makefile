CC ?= cc
CPPFLAGS ?= -I.
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror -pedantic

.PHONY: all test clean

all: mpaint-save

mpaint-save: main.o mpaint_save.o
	$(CC) $(CFLAGS) -o $@ $^

test-mpaint-save: test_mpaint_save.o mpaint_save.o
	$(CC) $(CFLAGS) -o $@ $^

test: test-mpaint-save
	./test-mpaint-save

clean:
	rm -f mpaint-save test-mpaint-save *.o
