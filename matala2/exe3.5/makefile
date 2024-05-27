CC = gcc
CFLAGS = -Wall -g

all: mync35 ttt

mync35: mync35.o
	$(CC) $(CFLAGS) mync35.o -o mync35

mync35.o: mync35.c
	$(CC) $(CFLAGS) -c mync35.c

ttt: ttt.o
	$(CC) $(CFLAGS) ttt.o -o ttt

ttt.o: ttt.c
	$(CC) $(CFLAGS) -c ttt.c

clean:
	rm -f *.o mync35 ttt
