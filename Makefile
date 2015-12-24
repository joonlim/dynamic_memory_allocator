CC=gcc
CFLAGS= -g -Werror -Wall
CNEXT=-DNEXT
CADDRESS=-DADDRESS
BIN=driver

all: $(BIN)

clean:
	rm -f *.o *.out $(BIN)

$(BIN): clean
	$(CC) $(CFLAGS) $(BIN).c -o $(BIN)

next: clean
	$(CC) $(CFLAGS) $(CNEXT) $(BIN).c -o $(BIN)

address: clean
	$(CC) $(CFLAGS) $(CADDRESS) $(BIN).c -o $(BIN)

both: clean
	$(CC) $(CFLAGS) $(CADDRESS) $(CNEXT) $(BIN).c -o $(BIN)

run: $(BIN)
	./$(BIN)

runnext: next
	./$(BIN)

runaddress: address
	./$(BIN)

runboth: both
	./$(BIN)
