
CFLAGS= -g -Wall -pedantic -pthread
CC=gcc

heatSim: main.o matrix2d.o leQueue.o mplib3.o
	$(CC) $(CFLAGS) -o heatSim main.o matrix2d.o leQueue.o mplib3.o

main.o: main.c matrix2d.h mplib3.h
	$(CC) $(CFLAGS) -c main.c

matrix2d.o: matrix2d.c matrix2d.h
	$(CC) $(CFLAGS) -c matrix2d.c

leQueue.o: leQueue.c leQueue.h
	$(CC) $(CFLAGS) -c leQueue.c

mplib3.o: mplib3.c mplib3.h leQueue.o
	$(CC) $(CFLAGS) -c mplib3.c

clean:
	rm -f *.o heatSim

run:
	./heatSim
