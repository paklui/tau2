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
 
include ../../include/Makefile 
include ../Makefile
include $(TAU_MAKEFILE_MPI_OPENMP)



CXX		= $(TAU_CXX)

CC		= $(TAU_CC)

CFLAGS          = $(TAU_INCLUDE) $(TAU_DEFS)  $(TAU_MPI_INCLUDE)

LIBS            = $(TAU_MPI_LIBS) $(TAU_LIBS) -lm

LDFLAGS	      	= $(CFLAGS)  $(TAU_LDFLAGS)

MAKEFILE      	= Makefile

PRINT	      	= pr

RM 	      	= /bin/rm -f

TARGET	      	= stommel

EXTRAOBJS     	= 

##############################################

all:		$(TARGET)	

install: 	$(TARGET)

$(TARGET):	$(TARGET).o 
	$(CXX) $(LDFLAGS) $(TARGET).o -o $@ $(LIBS)

$(TARGET).o : $(TARGET).c
	$(CC) $(CFLAGS) -c $(TARGET).c

clean: 	
	$(RM) $(TARGET).o $(TARGET) 
##############################################
