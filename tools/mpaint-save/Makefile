CC ?= cc
CPPFLAGS ?= -I.
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror -pedantic

.PHONY: all test clean

all: mpaint-save mpaint-save-rebuild

mpaint-save: main.o mpaint_save.o
	$(CC) $(CFLAGS) -o $@ $^

mpaint-save-rebuild: rebuild.o mpaint_encode.o mpaint_save.o
	$(CC) $(CFLAGS) -o $@ $^

test-mpaint-save: test_mpaint_save.o mpaint_save.o
	$(CC) $(CFLAGS) -o $@ $^

test-mpaint-encode: test_mpaint_encode.o mpaint_encode.o mpaint_save.o
	$(CC) $(CFLAGS) -o $@ $^

test: test-mpaint-save test-mpaint-encode
	./test-mpaint-save
	./test-mpaint-encode

clean:
	rm -f mpaint-save mpaint-save-rebuild test-mpaint-save test-mpaint-encode *.o
