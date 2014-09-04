CC=gcc
CFLAGS=-O3 -Wall
LDFLAGS=-lpthread
SRC_DIR=src/
BIN_DIR=bin/
HEADER_DIR=headers/

all: $(BIN_DIR)run

clean:
	@[ -d $(BIN_DIR) ] && echo "Cleaning." && rm -rf $(BIN_DIR) || echo "Nothing to clean."

define make_obj
	$(CC) $(CFLAGS) -o $(BIN_DIR)$(1).o -c $(SRC_DIR)$(1).c
endef

$(BIN_DIR)run: $(BIN_DIR)main.o $(BIN_DIR)errors.o $(BIN_DIR)common.o $(BIN_DIR)networking.o $(BIN_DIR)core.o $(BIN_DIR)commands.o
	$(CC) -o $(BIN_DIR)run $(^) $(LDFLAGS)

$(BIN_DIR)main.o: $(SRC_DIR)main.c $(HEADER_DIR)conf.h $(HEADER_DIR)errors.h $(HEADER_DIR)common.h $(HEADER_DIR)structures.h $(HEADER_DIR)commands.h
	[ ! -d bin ] && mkdir bin; true
	$(call make_obj,main)

$(BIN_DIR)errors.o: $(SRC_DIR)errors.c
	$(call make_obj,errors)

$(BIN_DIR)common.o: $(SRC_DIR)common.c $(HEADER_DIR)structures.h $(HEADER_DIR)errors.h $(HEADER_DIR)conf.h
	$(call make_obj,common)

$(BIN_DIR)networking.o: $(SRC_DIR)networking.c $(HEADER_DIR)errors.h $(HEADER_DIR)conf.h $(HEADER_DIR)structures.h $(HEADER_DIR)core.h
	$(call make_obj,networking)

$(BIN_DIR)commands.o: $(SRC_DIR)commands.c $(HEADER_DIR)structures.h $(HEADER_DIR)conf.h $(HEADER_DIR)core.h $(HEADER_DIR)common.h
	$(call make_obj,commands)

$(BIN_DIR)core.o: $(SRC_DIR)core.c $(HEADER_DIR)errors.h $(HEADER_DIR)conf.h $(HEADER_DIR)commands.h
	$(call make_obj,core)
