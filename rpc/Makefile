BIN := bin
GEN := gen
SRC := src
TST := tst

CFLAGS := -w
CC := gcc $(CFLAGS)
RPCGEN := rpcgen -C -N

all: clean compile

################
# Slave #
################

SLAVE_CLNT = $(BIN)/slave_clnt.o
SLAVE_SVC = $(BIN)/slave_svc.o
SLAVE_XDR = $(BIN)/slave_xdr.o

SLAVE_BIN = $(SLAVE_CLNT) $(SLAVE_SVC) $(SLAVE_XDR)
SLAVE_GEN = $(GEN)/slave_clnt.c $(GEN)/slave_svc.c $(GEN)/slave_xdr.c $(GEN)/slave.h

$(SLAVE_GEN): $(SRC)/slave.x
	@echo "Slave Gen"
	@cd src && $(RPCGEN) slave.x && mv slave.h slave_* ../gen/

$(SLAVE_CLNT): $(GEN)/slave_clnt.c $(GEN)/slave.h
	@echo "Compiling Slave Client"
	@$(CC) -c -o $(SLAVE_CLNT) $(GEN)/slave_clnt.c

$(SLAVE_SVC): $(GEN)/slave_svc.c $(GEN)/slave.h
	@echo "Compiling Slave Service"
	@$(CC) -c -o $(SLAVE_SVC) $(GEN)/slave_svc.c

$(SLAVE_XDR): $(GEN)/slave_xdr.c $(GEN)/slave.h
	@echo "Compiling Slave XDR"
	@$(CC) -c -o $(SLAVE_XDR) $(GEN)/slave_xdr.c

###########
# General #
###########

compile: $(SLAVE_BIN)

clean:
	@echo "Cleaning..."
	@if test -d $(BIN); then rm -rf $(BIN)/*; else mkdir $(BIN); fi
	@if test -d $(GEN); then rm -rf $(GEN)/*; else mkdir $(GEN); fi
