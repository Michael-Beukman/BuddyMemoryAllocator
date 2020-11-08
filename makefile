# Run using make test, or just make
CC=gcc
CFLAGS=-I.
DEPS = buddy.h buddy.c

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

test: build
	./main

clean:
	rm *.o main
build: buddy.o test.o 
	$(CC) -o main test.o buddy.o -lm

