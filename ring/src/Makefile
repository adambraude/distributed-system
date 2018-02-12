BIN := bin/

all: clean tree_map.o table.o

clean:
	@if test -d $(BIN); then rm -rf $(BIN)/*; else mkdir $(BIN); fi

table.o: table.c table.h
	@echo "Compiling table..."
	@gcc -c -Wall table.c -o bin/table.o

tree_map.o: tree_map.c tree_map.h
	@echo "Compiling tree map..."
	@gcc -c -Wall tree_map.c -o bin/tree_map.o
