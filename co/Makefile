CC=tcc
CFLAGS= -c -Wall -g
SRC:=$(wildcard *.c)
OBJ:=$(SRC:.c=.o)

all: co

clean:
	rm -f $(OBJ)
	
%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<	

co: $(OBJ)

