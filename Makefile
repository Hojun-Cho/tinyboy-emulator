CFLAGS= -Wall -g -Wextra  -g

all: gb

clean:
	rm -f *.o
	rm -f gb
	
gb: $(OBJ)
	$(CC) jmp.S *.c -lSDL2 -o gb $(CFLAGS) 
