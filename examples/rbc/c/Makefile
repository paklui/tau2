#****************************************************************************
#*			TAU Portable Profiling Package			   **
#*			http://www.cs.uoregon.edu/research/tau	           **
#****************************************************************************
#*    Copyright 1997  						   	   **
#*    Department of Computer and Information Science, University of Oregon **
#*    Advanced Computing Laboratory, Los Alamos National Laboratory        **
#****************************************************************************
#######################################################################
##                  pC++/Sage++  Copyright (C) 1993,1995             ##
##  Indiana University  University of Oregon  University of Rennes   ##
#######################################################################

EXE = simple

SRC = $(EXE).c
OBJ = $(SRC:.c=.o)

CC = tau_cc.sh
LD = tau_cc.sh

CFLAGS  = -O2 -g -optKeepFiles
LDFLAGS = 

RM = /bin/rm -f


##############################################

all: static dynamic
	@echo "Please make run-static or run-dynamic"

run-static: static
	bash -c "TAU_MEMDBG_PROTECT_ABOVE=1 TAU_TRACK_MEMORY_LEAKS=1 TAU_TRACK_HEAP=1 ./$(EXE)-static"

run-dynamic: dynamic
	tau_exec -memory_debug -T serial,pdt ./$(EXE)-dynamic

static: $(OBJ)
	$(LD) -optMemDbg $(LDFLAGS) $(OBJ) -o $(EXE)-static

dynamic: $(OBJ)
	$(LD) $(LDFLAGS) $(OBJ) -o $(EXE)-dynamic

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean: 	
	$(RM) $(OBJ) $(EXE)-static $(EXE)-dynamic
	$(RM) *.inst.* *.pdb profile.*
