CC = gcc
CFLAGS = -c


desafio1: desafio1.o
	$(CC) desafio1.o -o desafio1
desafio1.o: desafio1.c
	$(CC) $(CFLAGS) desafio1.c
clean:
	$(RM) *.o desafio1 core