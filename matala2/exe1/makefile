
CC = gcc               
CFLAGS = -Wall -g   

all: ttt 

ttt: ttt.o
	$(CC) $(CFLAGS) ttt.o -o ttt  

ttt.o: ttt.c 
	$(CC) $(CFLAGS) -c ttt.c

clean:
	rm -f *.o ttt 