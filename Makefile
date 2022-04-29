CC = gcc
CFLAGS=-I.

mysh: mysh.o
	$(CC) -o mysh mysh.c
