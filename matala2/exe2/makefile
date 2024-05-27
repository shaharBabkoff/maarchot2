
CC = gcc               
CFLAGS = -Wall -g   

all: mync ttt


mync: mync.o
	$(CC) $(CFLAGS) mync.o -o mync  

mync.o: mync.c 
	$(CC) $(CFLAGS) -c mync.c

ttt: ttt.o
	$(CC) $(CFLAGS) ttt.o -o ttt  

ttt.o: ttt.c 
	$(CC) $(CFLAGS) -c ttt.c


clean:
	rm -f *.o mync ttt