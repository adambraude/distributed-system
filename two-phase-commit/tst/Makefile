BIN := bin

all: integ_test
	@bin/integ_test

integ_test: integ_test.c integ_test.h
	@gcc -Wall -o $(BIN)/integ_test integ_test.c

clean:
	@if test -d $(BIN); then rm -rf $(BIN)/*; else mkdir $(BIN); fi
