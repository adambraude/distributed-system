MSTR = tpc_master
SLV = tpc_slave
CFLAGS = -g -Wall

MASTER_OBJ =  tpc_clnt.o tpc_xdr.o tpc_master.o
SLAVE_OBJ = tpc_slave.o tpc_xdr.o tpc_svc.o

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

clean:
	rm tpc_clnt.c tpc_svc.c tpc_xdr.c tpc.h *.o $(MSTR) $(SLV) vectors.txt
