/****************************************************************************
**			TAU Portable Profiling Package			   **
**			http://www.acl.lanl.gov/tau		           **
*****************************************************************************
**    Copyright 1997  						   	   **
**    Department of Computer and Information Science, University of Oregon **
**    Advanced Computing Laboratory, Los Alamos National Laboratory        **
****************************************************************************/
/***************************************************************************
**	File 		: RtsLayer.h					  **
**	Description 	: TAU Profiling Package Runtime System Layer	  **
**	Author		: Sameer Shende					  **
**	Contact		: sameer@cs.uoregon.edu sameer@acl.lanl.gov 	  **
**	Flags		: Compile with				          **
**			  -DPROFILING_ON to enable profiling (ESSENTIAL)  **
**			  -DPROFILE_STATS for Std. Deviation of Excl Time **
**			  -DSGI_HW_COUNTERS for using SGI counters 	  **
**			  -DPROFILE_CALLS  for trace of each invocation   **
**			  -DSGI_TIMERS  for SGI fast nanosecs timer	  **
**			  -DTULIP_TIMERS for non-sgi Platform	 	  **
**			  -DPOOMA_STDSTL for using STD STL in POOMA src   **
**			  -DPOOMA_TFLOP for Intel Teraflop at SNL/NM 	  **
**			  -DPOOMA_KAI for KCC compiler 			  **
**			  -DDEBUG_PROF  for internal debugging messages   **
**                        -DPROFILE_CALLSTACK to enable callstack traces  **
**	Documentation	: See http://www.acl.lanl.gov/tau	          **
***************************************************************************/

#ifndef _RTSLAYER_H_
#define _RTSLAYER_H_

//////////////////////////////////////////////////////////////////////
//
// class RtsLayer
//
// This class is used for porting the TAU Profiling package to other
// platforms and software frameworks. It contains functions to get
// the node id, thread id etc. When Threads are implemented, myThread()
// method should return the thread id in {0..N-1} where N is the total
// number of threads. All interaction with the outside world should be
// restrained to this class. 
//////////////////////////////////////////////////////////////////////

class RtsLayer 
{ // Layer for Profiler to interact with the Runtime System
  public:
 	
 	RtsLayer () { }  // defaults
	~RtsLayer () { } 

	static TauGroup_t & TheProfileMask(void);
	
	static int& TheNode(void);
  
	static int& TheContext(void);

 	static TauGroup_t  enableProfileGroup(TauGroup_t  ProfileGroup) ;

        static TauGroup_t  resetProfileGroup(void) ;

	static int setAndParseProfileGroups (char *prog, char *str) ;

        static bool isEnabled(TauGroup_t  ProfileGroup) ; 

        static void ProfileInit(int argc, char **argv);

	static string PrimaryGroup(const char *ProfileGroupName);

        static bool isCtorDtor(const char *name);
	
	static void TraceSendMsg(int type, int destination, int length);
	static void TraceRecvMsg(int type, int source, int length);

	inline
	static const char * CheckNotNull(const char * str) {
  	  if (str == 0) return "  ";
          else return str;
	}


  	static int 	SetEventCounter(void);
  	static double 	GetEventCounter(void);

	static double   getUSecD(int tid); 

	static int 	setMyNode(int NodeId, int tid=RtsLayer::myThread());

	static int 	setMyContext(int ContextId);

	// For tracing 
	static int 	DumpEDF(int tid); 

  	// Return the number of the 'current' node.
	static int myNode(void);

	// Return the number of the 'current' context.
	static int myContext(void);

	// Return the number of the 'current' thread. 0..TAU_MAX_THREADS-1
	static int myThread(void);

	static void RegisterThread();

	// This ensure that the FunctionDB (global) is locked while updating
	static void LockDB(void);
	
	static void UnLockDB(void);

}; 

#endif /* _RTSLAYER_H_  */
/***************************************************************************
 * $RCSfile: RtsLayer.h,v $   $Author: sameer $
 * $Revision: 1.7 $   $Date: 1999/08/20 22:55:22 $
 * POOMA_VERSION_ID: $Id: RtsLayer.h,v 1.7 1999/08/20 22:55:22 sameer Exp $ 
 ***************************************************************************/
