CC = gcc
CFLAGS = -Wall -g

all: mync35 

mync35: mync35.o
	$(CC) $(CFLAGS) mync35.o -o mync35

mync35.o: mync35.c
	$(CC) $(CFLAGS) -c mync35.c

clean:
	rm -f *.o mync35 
