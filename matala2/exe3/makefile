CC = gcc
CFLAGS = -Wall -g

all: mync3 ttt

mync3: mync3.o
	$(CC) $(CFLAGS) mync3.o -o mync3

mync3.o: mync3.c
	$(CC) $(CFLAGS) -c mync3.c

ttt: ttt.o
	$(CC) $(CFLAGS) ttt.o -o ttt

ttt.o: ttt.c
	$(CC) $(CFLAGS) -c ttt.c

clean:
	rm -f *.o mync3 ttt
