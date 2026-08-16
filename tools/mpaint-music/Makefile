CC ?= cc
CPPFLAGS ?= -I.
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror -pedantic

.PHONY: all test clean

all: mpaint-music

mpaint-music: main.o mpaint_music.o
	$(CC) $(CFLAGS) -o $@ $^

test-mpaint-music: test_mpaint_music.o mpaint_music.o
	$(CC) $(CFLAGS) -o $@ $^

test: test-mpaint-music
	./test-mpaint-music

clean:
	rm -f mpaint-music test-mpaint-music *.o
