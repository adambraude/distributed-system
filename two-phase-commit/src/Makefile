BIN := bin/

MSTR = $(BIN)tpc_master
SLV = $(BIN)tpc_slave

CFLAGS = -g -Wall

MASTER_OBJ =  $(BIN)tpc_clnt.o $(BIN)tpc_xdr.o $(BIN)tpc_master.o
SLAVE_OBJ = $(BIN)tpc_slave.o $(BIN)tpc_xdr.o $(BIN)tpc_svc.o

all:
	@rpcgen tpc.x
	@make objects

objects: $(SLV) $(MSTR)

$(MSTR): $(MASTER_OBJ) tpc.h vote.h
	@echo "compiling master"
	gcc -o $(MSTR) $(MASTER_OBJ) -lpthread

$(SLV): $(SLAVE_OBJ) tpc.h vote.h
	@echo "compiling slave"
	gcc -o $(SLV) $(SLAVE_OBJ)

$(SLAVE_OBJ): tpc_slave.c tpc_xdr.c tpc_svc.c
	@gcc -c -Wall -o $(BIN)tpc_slave.o tpc_slave.c
	@gcc -c -Wall -o $(BIN)tpc_xdr.o tpc_xdr.c
	@gcc -c -Wall -o $(BIN)tpc_svc.o tpc_svc.c

$(MASTER_OBJ): tpc_clnt.c tpc_master.c
	@gcc -c -Wall -o $(BIN)tpc_clnt.o tpc_clnt.c
	@gcc -c -Wall -o $(BIN)tpc_master.o tpc_master.c

clean:
	@if test -d $(BIN); then rm -rf $(BIN)/*; else mkdir $(BIN); fi
