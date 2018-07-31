########
# DBIE #
########

.PHONY: all
all: slave master dbms

.PHONY: clean
clean:
	@echo "Cleaning..."
	@if test -d $(BIN); then rm -rf $(BIN)/*; else mkdir $(BIN); fi
	@if test -d $(RPC_GEN); then rm -rf $(RPC_GEN)/*; else mkdir $(RPC_GEN); fi

#############
# Variables #
#############

BIN := bin
RPC_GEN := rpc/gen
BITMAP_ENGINE := ../bitmap-engine/BitmapEngine/src

CFLAGS := -Wall -fno-stack-protector
CIGNORE := -Wno-incompatible-pointer-types \
	-Wno-unused-variable -Wno-unused-but-set-variable
CC := @gcc $(CFLAGS) $(CIGNORE)

BE_OBJS := \
	$(BIN)/SegUtil.o \
	$(BIN)/WAHQuery.o

DS_OBJS := \
	$(BIN)/ipc_util.o \
	$(BIN)/master_rq.o \
	$(BIN)/master_tpc.o \
	$(BIN)/read_vec.o \
	$(BIN)/slavelist.o \
	$(BIN)/tree_map.o

RPC_OBJS := \
	$(BIN)/slave_clnt.o \
	$(BIN)/slave_svc.o \
	$(BIN)/slave_xdr.o

OBJECTS := $(BE_OBJS) $(DS_OBJS) $(RPC_OBJS)

############
# Binaries #
############

.PHONY: dbms
dbms: $(BIN)/dbms
$(BIN)/dbms: $(BIN)/read_vec.o $(BIN)/dbms-helper.o $(BIN)/ipc_util.o dbms/dbms.c
	@echo "Compiling DBMS"
	$(CC) -o $(BIN)/dbms \
		$(BIN)/read_vec.o \
		$(BIN)/dbms-helper.o \
		$(BIN)/ipc_util.o \
		dbms/dbms.c -lm

.PHONY: master
master: $(BIN)/master
$(BIN)/master: $(OBJECTS) master/master.c
	@echo "Compiling Master"
	@cp query-planner/iter-mst/mst_planner.py $(BIN)
	$(CC) -o $(BIN)/master \
		$(BIN)/slave_clnt.o \
		$(BIN)/slave_xdr.o \
		$(BIN)/tree_map.o \
		$(BIN)/master_rq.o \
		$(BIN)/master_tpc.o \
		$(BIN)/WAHQuery.o \
		$(BIN)/SegUtil.o \
		$(BIN)/read_vec.o \
		$(BIN)/slavelist.o \
		$(BIN)/ipc_util.o \
		master/master.c \
		-lssl -lpthread -lcrypto -lm # TODO: make `MASTER_FLAGS` target

.PHONY: slave
slave: $(BIN)/slave
$(BIN)/slave: slave/slave.c slave/slave.h $(RPC_GEN)/slave.h $(OBJECTS)
	@echo "Compiling Slave"
	$(CC) -o $(BIN)/slave \
		$(BIN)/slave_clnt.o \
		$(BIN)/slave_svc.o \
		$(BIN)/slave_xdr.o \
		$(BIN)/WAHQuery.o \
		$(BIN)/SegUtil.o \
		$(BIN)/read_vec.o \
		$(BIN)/slavelist.o \
		slave/slave.c \
		-lm

#########################
# Bitmap Engine Objects #
#########################

$(BIN)/SegUtil.o: $(BITMAP_ENGINE)/seg-util/SegUtil.c
	@echo "OBJ: Compiling $@"
	$(CC) -c -o $(BIN)/SegUtil.o $(BITMAP_ENGINE)/seg-util/SegUtil.c

$(BIN)/WAHQuery.o: $(BITMAP_ENGINE)/wah/WAHQuery.c
	@echo "OBJ: Compiling $@"
	$(CC) -c -o $(BIN)/WAHQuery.o $(BITMAP_ENGINE)/wah/WAHQuery.c

##############################
# Distributed System Objects #
##############################

$(BIN)/dbms-helper.o: bitmap-vector/read_vec.h ipc/messages.h types/types.h dbms/dbms-helper.h dbms/dbms-helper.c
	@echo "OBJ: Compiling $@"
	$(CC) -c -o $(BIN)/dbms-helper.o dbms/dbms-helper.c

$(BIN)/ipc_util.o: util/ipc_util.c
	@echo "OBJ: Compiling $@"
	$(CC) -c -o $(BIN)/ipc_util.o util/ipc_util.c

$(BIN)/master_rq.o: master/master_rq.c $(RPC_GEN)/slave.h
	@echo "OBJ: Compiling $@"
	$(CC) -c -o $(BIN)/master_rq.o master/master_rq.c

$(BIN)/master_tpc.o: master/master_tpc_vector.c $(RPC_GEN)/slave.h
	@echo "OBJ: Compiling $@"
	$(CC) -c -o $(BIN)/master_tpc.o master/master_tpc_vector.c

$(BIN)/read_vec.o: bitmap-vector/read_vec.c
	@echo "OBJ: Compiling $@"
	$(CC) -c -o $(BIN)/read_vec.o bitmap-vector/read_vec.c

$(BIN)/slavelist.o: util/slavelist.c
	@echo "OBJ: Compiling $@"
	$(CC) -c -o $(BIN)/slavelist.o util/slavelist.c

$(BIN)/tree_map.o: consistent-hash/ring/src/tree_map.c
	@echo "OBJ: Compiling $@"
	$(CC) -c -o $(BIN)/tree_map.o consistent-hash/ring/src/tree_map.c

###############
# RPC Objects #
###############

RPC_GEN_OUT := \
	$(RPC_GEN)/slave_clnt.c \
	$(RPC_GEN)/slave_svc.c \
	$(RPC_GEN)/slave_xdr.c \
	$(RPC_GEN)/slave.h

$(RPC_GEN_OUT): rpc/src/slave.x
	@echo "RPC: Generating Source Files"
	@cd rpc/src && rpcgen -N -C slave.x
	@mv rpc/src/slave.h $(RPC_GEN)
	@mv rpc/src/*.c $(RPC_GEN)

$(BIN)/slave_clnt.o: $(RPC_GEN)/slave_clnt.c $(RPC_GEN)/slave.h
	@echo "RPC: Compiling $@"
	$(CC) -c -o $(BIN)/slave_clnt.o $(RPC_GEN)/slave_clnt.c

$(BIN)/slave_svc.o: $(RPC_GEN)/slave_svc.c $(RPC_GEN)/slave.h
	@echo "RPC: Compiling $@"
	$(CC) -c -o $(BIN)/slave_svc.o $(RPC_GEN)/slave_svc.c

$(BIN)/slave_xdr.o: $(RPC_GEN)/slave_xdr.c $(RPC_GEN)/slave.h
	@echo "RPC: Compiling $@"
	$(CC) -c -o $(BIN)/slave_xdr.o $(RPC_GEN)/slave_xdr.c

###############
# Convenience #
###############

.PHONY: spawn_slave
spawn_slave: slave
	@echo "Starting Slave"
	@rpcbind
	@cd $(BIN) && ./slave 1> out.log 2> err.log

.PHONY: start_dbms
start_dbms: dbms master
	@echo "Starting DBMS"
	@cd $(BIN) && ./dbms

######################
# Centralized Master #
######################

.PHONY: start_master_cent
start_master_cent: master_cent
	@echo "Starting Centralized Master"
	@cd $(BIN) && ./master_cent

.PHONY: master_cent
master_cent: $(BIN)/master_cent
$(BIN)/master_cent: $(OBJECTS) $(BIN)/master_rq_cent.o
	@echo "Compiling Centralized Master"
	$(CC) -o $(BIN)/master_cent \
		$(BIN)/master_rq_cent.o \
		$(BIN)/WAHQuery.o \
		$(BIN)/SegUtil.o \
		$(BIN)/read_vec.o \
		master/master_cent.c \
		-lpthread

$(BIN)/master_rq_cent.o: master/master_rq_cent.c $(RPC_GEN)/slave.h
	$(CC) -c -o $(BIN)/master_rq_cent.o master/master_rq_cent.c

#############
# SLAVELIST #
#############

SLAVELIST:
	bash create-slavelist.sh 1022

#######################
# Directory Existence #
#######################

$(shell mkdir -p $(BIN))
$(shell mkdir -p $(RPC_GEN))
