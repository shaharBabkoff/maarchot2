CC = gcc
CFLAGS = -Wall -g

all: mync5 ttt

mync5: mync5.o
	$(CC) $(CFLAGS) mync5.o -o mync5

mync5.o: mync5.c
	$(CC) $(CFLAGS) -c mync5.c
ttt: ttt.o
	$(CC) $(CFLAGS) ttt.o -o ttt

ttt.o: ttt.c
	$(CC) $(CFLAGS) -c ttt.c

clean:
	rm -f *.o mync5 ttt
