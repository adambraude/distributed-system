BIN := bin
SRC := src
TST := tst

# Clean output, (re)compile as necessary, and test.
all : clean compile test

# Clean out the `bin/` directory.
clean :
	@echo "Cleaning..."
	@if test -d $(BIN); then rm -rf $(BIN)/*; else mkdir $(BIN); fi

# If/as more binaries are needed, add here to maintain a simple `all` rule.
compile :
	@echo "Compiling..."
	@make $(BIN)/jump_test.o

# Run testing.
test :
	@echo "Testing...\n"
	@$(BIN)/jump_test.o

# Compile the testing binary.
$(BIN)/jump_test.o : $(SRC)/jump.h $(SRC)/jump.c $(TST)/jump_test.h $(TST)/jump_test.c
	@gcc -Wall -o $(BIN)/jump_test.o $(TST)/jump_test.c -lm
