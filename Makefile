CC=tcc
CFLAGS= -c -Wall -g -Wextra 
SRC:=$(wildcard *.c)
OBJ:=$(SRC:.c=.o)

all: gb

clean:
	make clean -C co/
	rm -f $(OBJ)
	rm -f gb
	
%.o: %.c gb.h
	$(CC) $(CFLAGS) -o $@ $<	

gb: $(OBJ)
	make all -C co/
	$(CC) co/*.o *.o -lucontext -lSDL2 -g  -o gb
