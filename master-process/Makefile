# Directories
BIN := bin
SRC := src
TST := tst

# Compilation Flags
CFLAGS := -Wall -Wextra -pedantic

# Master Process
MASTER := $(BIN)/master
MASTER_DEPS := $(SRC)/master.c $(SRC)/master.h

# DBMS Imitation Processs
DBMS := $(BIN)/dbms
DBMS_DEPS := $(SRC)/dbms.c $(SRC)/dbms.h

# Resulting Binaries
OBJECTS := $(MASTER) $(DBMS)

all: clean compile test

clean:
	@echo "Cleaning..."
	@if test -d $(BIN); then rm -rf $(BIN)/*; else mkdir $(BIN); fi

compile: $(OBJECTS)
	@echo "Compiling..."

$(MASTER): $(MASTER_DEPS)
	@echo "\tCompiling Master..."
	gcc $(CFLAGS) -o $(MASTER) $(SRC)/master.c

$(DBMS): $(DBMS_DEPS)
	@echo "\tCompiling DBMS..."
	gcc $(CFLAGS) -o $(DBMS) $(SRC)/dbms.c

test:
	@echo "Testing..."
	@cd bin && ./dbms
