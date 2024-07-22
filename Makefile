CC=tcc
CFLAGS= -c -Wall -g -Wextra 

all: gb

clean:
	rm -f *.o
	rm -f gb
	
gb: $(OBJ)
	$(CC) jmp.S *.c -lSDL2 -o gb
