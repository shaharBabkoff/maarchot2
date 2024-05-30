CC = gcc
CFLAGS = -Wall -g

all: mync4 ttt

mync4: mync4.o
	$(CC) $(CFLAGS) mync4.o -o mync4

myn4.o: mync4.c
	$(CC) $(CFLAGS) -c mync4.c

ttt: ttt.o
	$(CC) $(CFLAGS) ttt.o -o ttt

ttt.o: ttt.c
	$(CC) $(CFLAGS) -c ttt.c

clean:
	rm -f *.o mync4 ttt
