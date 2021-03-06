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
include $(TAU_MAKEFILE_PDT)


CXX		= $(TAU_CXX)

CC		= $(TAU_CC)

PDTPARSE 	= $(PDTDIR)/$(PDTARCHDIR)/bin/cxxparse

TAUINSTR 	= $(TAU_PREFIX_INSTALL_DIR)/$(CONFIG_ARCH)/bin/tau_instrumentor

CFLAGS        	= $(TAU_INCLUDE) $(TAU_DEFS) -UTAU_MPI

LIBS 	      	= $(TAU_LIBS) 

LDFLAGS	      	= $(USER_OPT) $(TAU_DEFS)

MAKEFILE      	= Makefile

PRINT	      	= pr

RM 	      	= /bin/rm -f

TARGET	      	= klargest

EXTRAOBJS     	= 

##############################################
# Original Rules
##############################################
#all:		$(TARGET)	
#
#install: 	$(TARGET)
#
#$(TARGET):	$(TARGET).o
#	$(CXX) $(LDFLAGS) $(TARGET).o -o $@ $(LIBS)
#
#$(TARGET).o : $(TARGET).cpp
#	$(CXX) $(CFLAGS) -c $(TARGET).cpp
#
#clean: 	
#	$(RM) $(TARGET).o $(TARGET)
##############################################



##############################################
# Modified Rules
##############################################

all: 	$(TARGET) $(PDTPARSE) $(TAUINSTR)

$(TARGET): $(TARGET).o 
	$(CXX) $(LDFLAGS) $(TARGET).o -o $@ $(LIBS)

# Use the instrumented source code to generate the object code
$(TARGET).o : $(TARGET).inst.cpp 
	$(CXX) -c $(CFLAGS) $(TARGET).inst.cpp  -o $(TARGET).o

# Generate the instrumented source from the original source and the pdb file
$(TARGET).inst.cpp : $(TARGET).pdb $(TARGET).cpp $(TAUINSTR) 
	$(TAUINSTR) $(TARGET).pdb $(TARGET).cpp -o $(TARGET).inst.cpp -f select 

# Parse the source file to generate the pdb file
$(TARGET).pdb : $(PDTPARSE) $(TARGET).cpp 
	$(PDTPARSE) $(TARGET).cpp $(CFLAGS) 

# When implicit rules are used in Makefiles, we can change the original rule
#.C.o:
#        $(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@ ;
#  to 
#.C.o:
#       $(PDTPARSE) $< $(CXXFLAGS) $(CPPFLAGS) 
#       $(TAUINSTR) $*.pdb $< -o $*.inst.C ; 
#	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $*.inst.C -o $@ ; 
#       rm -f $*.pdb ;      
# The same procedure is applicable to C and F90 source files as well.


clean: 
	$(RM) $(TARGET).o $(TARGET).inst.cpp $(TARGET) $(TARGET).pdb
##############################################

$(PDTPARSE):
	@echo "*********************************************************"
	@echo "Download and Install Program Database Toolkit "
	@echo "ERROR: Cannot find $(PDTPARSE)"
	@echo "*********************************************************"

$(TAUINSTR):
	@echo "*********************************************************"
	@echo "Configure TAU with -pdt=<dir> configuration option to use" 
	@echo "C++ instrumentation with PDT"
	@echo "ERROR: Cannot find $(TAUINSTR)"
	@echo "*********************************************************"

