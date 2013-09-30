# --------------------------------------------------------------------------
#
# MAKEFILE for fd-multiplex
#
# --------------------------------------------------------------------------
# Data
GCC?=gcc
SRC=$(CURDIR)/src
BIN=$(CURDIR)/bin
OBJ=$(BIN)/obj
LIB=$(BIN)/fd-multiplex.a

# Files
SOURCES=$(shell find "$(SRC)" -type f -name "*.c")
OBJECTS=$(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES))
CINCLUDES=-I "$(SRC)"
CLIBRARIES=-lpthread

# Example
EX=$(CURDIR)/example
EX_SRC=$(shell find "$(EX)" -type f -name "*.c")
EX_DST=$(patsubst $(EX)/%.c, $(BIN)/%.example, $(EX_SRC))

# --------------------------------------------------------------------------
# Targets
all: init $(LIB)
init: $(BIN) $(OBJ)
clean:; rm -rf "$(BIN)";
example: $(EX_DST)

# Files/Directories
$(BIN): 
	mkdir -p "$(BIN)"
$(OBJ):
	mkdir -p "$(OBJ)"
$(LIB): $(BIN) $(OBJECTS)
	ar rcs "$(LIB)" $(OBJECTS)
$(OBJECTS): $(OBJ)/%.o: $(SRC)/%.c $(OBJ)
	$(GCC) $(CFLAGS) $(CINCLUDES) -c $< -o $@ $(CLIBRARIES)
$(EX_DST): $(BIN)/%.example: $(EX)/%.c $(LIB) $(BIN)
	$(GCC) $(CFLAGS) $(CINCLUDES) -o $@ $< $(LIB) $(CLIBRARIES)

# Example
