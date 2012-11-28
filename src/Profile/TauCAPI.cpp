/****************************************************************************
**			TAU Portable Profiling Package			   **
**			http://www.cs.uoregon.edu/research/tau	           **
*****************************************************************************
**    Copyright 1997  						   	   **
**    Department of Computer and Information Science, University of Oregon **
**    Advanced Computing Laboratory, Los Alamos National Laboratory        **
****************************************************************************/
/***************************************************************************
**	File 		: TauCAPI.C					  **
**	Description 	: TAU Profiling Package API wrapper for C	  **
**	Contact		: tau-team@cs.uoregon.edu 		 	  **
**	Documentation	: See http://www.cs.uoregon.edu/research/tau      **
***************************************************************************/

#ifdef TAU_DOT_H_LESS_HEADERS 
#include <iostream>
using namespace std;
#else /* TAU_DOT_H_LESS_HEADERS */
#include <iostream.h>
#endif /* TAU_DOT_H_LESS_HEADERS */
#include "Profile/Profiler.h"
#include <Profile/TauSampling.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <Profile/TauMetrics.h>
#include <Profile/TauSnapshot.h>
#include <Profile/TauTrace.h>

#if (!defined(TAU_WINDOWS)) 
/* Needed for fork */
#include <sys/types.h>
#include <unistd.h>
#endif /* TAU_WINDOWS */

#if defined(TAUKTAU)
#include <Profile/KtauProfiler.h>
#endif //TAUKTAU


#ifdef TAU_EPILOG
#include "elg_trc.h"

#ifdef TAU_SCALASCA
extern "C" {
void esd_enter (elg_ui4 rid);
void esd_exit (elg_ui4 rid);
}
#endif /* SCALASCA */
#endif /* TAU_EPILOG */


#ifdef TAU_VAMPIRTRACE
#include "Profile/TauVampirTrace.h"
#endif /* TAU_VAMPIRTRACE */

#ifdef TAU_SCOREP
#include <Profile/TauSCOREP.h>
#endif


extern "C" void * Tau_get_profiler(const char *fname, const char *type, TauGroup_t group, const char *gr_name) {
  FunctionInfo *f;

  Tau_global_incr_insideTAU();

  DEBUGPROFMSG("Inside get_profiler group = " << group<<endl;);

  // since we're using new, we should set InitData to true in FunctionInfoInit
  if (group == TAU_MESSAGE) {
    if (gr_name && strcmp(gr_name, "TAU_MESSAGE") == 0) {
      f = new FunctionInfo(fname, type, group, "MPI", true);
    } else {
      f = new FunctionInfo(fname, type, group, gr_name, true);
    }
  } else {
    f = new FunctionInfo(fname, type, group, gr_name, true);
  }

  Tau_global_decr_insideTAU();

  return (void *) f;
}



#define STACK_DEPTH_INCREMENT 100
static Profiler *Tau_global_stack[TAU_MAX_THREADS];
static int Tau_global_stackdepth[TAU_MAX_THREADS];
static int Tau_global_stackpos[TAU_MAX_THREADS];
static int Tau_global_insideTAU[TAU_MAX_THREADS];
static int Tau_is_thread_fake_for_task_api[TAU_MAX_THREADS];
int lightsOut = 0;

static void (*_profile_write_hook)(void) = NULL;

extern "C" void Tau_global_addWriteHook(void (*hook)(void)) {
  _profile_write_hook = hook;
}

extern "C" void Tau_global_callWriteHooks() {
  if (_profile_write_hook != NULL) {
    _profile_write_hook();
  }
}


static void Tau_stack_checkInit() {
  static int init = 0;
  if (init != 0) {
    return;
  }
  init = 1;

  lightsOut = 0;
  _profile_write_hook = 0;

  for (int i=0; i<TAU_MAX_THREADS; i++) {
    Tau_global_stackdepth[i] = 0;
    Tau_global_stackpos[i] = -1;
    Tau_global_stack[i] = NULL;
    Tau_global_insideTAU[i] = 0;
    Tau_is_thread_fake_for_task_api[i] = 0; /* by default all threads are real*/
  }
}

extern "C" int Tau_global_getLightsOut() {
  Tau_stack_checkInit();
  return lightsOut;
}

extern "C" void Tau_global_setLightsOut() {
  Tau_stack_checkInit();
  lightsOut = 1;
}

/* the task API does not have a real thread associated with the tid */
extern "C" int Tau_is_thread_fake(int tid) {
  return Tau_is_thread_fake_for_task_api[tid]; 
}

extern "C" void Tau_set_thread_fake(int tid) {
  Tau_is_thread_fake_for_task_api[tid] = 1; 
}

extern "C" void Tau_stack_initialization() {
  Tau_stack_checkInit();
}

extern "C" int Tau_global_get_insideTAU() {
  Tau_stack_checkInit();
  int tid = RtsLayer::myThread();
  return Tau_global_insideTAU[tid];
}

extern "C" int Tau_global_get_insideTAU_tid(int tid) {
  Tau_stack_checkInit();
  return Tau_global_insideTAU[tid];
}

extern "C" int Tau_global_incr_insideTAU() {
  Tau_stack_checkInit();
  int tid = RtsLayer::myThread();
  Tau_global_insideTAU[tid]++;
  return Tau_global_insideTAU[tid];
}

extern "C" int Tau_global_decr_insideTAU() {
  Tau_stack_checkInit();
  int tid = RtsLayer::myThread();
  Tau_global_insideTAU[tid]--;
  return Tau_global_insideTAU[tid];
}

extern "C" int Tau_global_incr_insideTAU_tid(int tid) {
  Tau_stack_checkInit();
  Tau_global_insideTAU[tid]++;
  return Tau_global_insideTAU[tid];
}

extern "C" int Tau_global_decr_insideTAU_tid(int tid) {
  Tau_stack_checkInit();
  Tau_global_insideTAU[tid]--;
  return Tau_global_insideTAU[tid];
}

extern "C" Profiler *TauInternal_CurrentProfiler(int tid) {
  int pos = Tau_global_stackpos[tid];
  if (pos < 0) {
    return NULL;
  }
  return &(Tau_global_stack[tid][pos]);
}

extern "C" Profiler *TauInternal_ParentProfiler(int tid) {
  int pos = Tau_global_stackpos[tid]-1;
  if (pos < 0) {
    return NULL;
  }
  return &(Tau_global_stack[tid][pos]);
}


///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_start_timer(void *functionInfo, int phase, int tid) {
  Tau_global_insideTAU[tid]++;

  //int tid = RtsLayer::myThread();
  FunctionInfo *fi = (FunctionInfo *) functionInfo; 

  if ( !RtsLayer::TheEnableInstrumentation() || !(fi->GetProfileGroup() & RtsLayer::TheProfileMask())) {
    Tau_global_insideTAU[tid]--;
    return; /* disabled */
  }

#ifndef TAU_WINDOWS
  if (TauEnv_get_ebs_enabled()) {
    Tau_sampling_init_if_necessary();
    Tau_sampling_suspend(tid);
  }
#endif

#ifdef TAU_TRACK_IDLE_THREADS
  /* If we are performing idle thread tracking, we start a top level timer */
  if (tid != 0) {
    Tau_create_top_level_timer_if_necessary();
  }
#endif


#ifdef TAU_EPILOG
  esd_enter(fi->GetFunctionId());
#ifndef TAU_WINDOWS
  if (TauEnv_get_ebs_enabled()) {
    Tau_sampling_resume(tid);
  }
#endif
  Tau_global_insideTAU[tid]--;
  return;
#endif

#ifdef TAU_VAMPIRTRACE 
  uint64_t TimeStamp = vt_pform_wtime();
#ifdef TAU_VAMPIRTRACE_5_12_API
  vt_enter(VT_CURRENT_THREAD, (uint64_t *) &TimeStamp, fi->GetFunctionId());
#else
  vt_enter((uint64_t *) &TimeStamp, fi->GetFunctionId());
#endif /* TAU_VAMPIRTRACE_5_12_API */
#ifndef TAU_WINDOWS
  if (TauEnv_get_ebs_enabled()) {
    Tau_sampling_resume(tid);
  }
#endif
  Tau_global_insideTAU[tid]--;
  return;
#endif

#ifdef TAU_SCOREP
  SCOREP_Tau_EnterRegion(fi->GetFunctionId());
#endif


  // move the stack pointer
  Tau_global_stackpos[tid]++; /* push */



  if (Tau_global_stackpos[tid] >= Tau_global_stackdepth[tid]) {
    int oldDepth = Tau_global_stackdepth[tid];
    int newDepth = oldDepth + STACK_DEPTH_INCREMENT;
    Profiler *newStack = (Profiler *) malloc(sizeof(Profiler)*newDepth);
    memcpy(newStack, Tau_global_stack[tid], oldDepth*sizeof(Profiler));
    Tau_global_stack[tid] = newStack;
    Tau_global_stackdepth[tid] = newDepth;
  }

  Profiler *p = &(Tau_global_stack[tid][Tau_global_stackpos[tid]]);

  p->MyProfileGroup_ = fi->GetProfileGroup();
  p->ThisFunction = fi;
  p->needToRecordStop = 0;

#ifdef TAU_MPITRACE
  p->RecordEvent = false; /* by default, we don't record this event */
#endif /* TAU_MPITRACE */

  
#ifdef TAU_PROFILEPHASE
  if (phase) {
    p->SetPhase(true);
  } else {
    p->SetPhase(false);
  }
#endif /* TAU_PROFILEPHASE */

#ifdef TAU_DEPTH_LIMIT
  static int userspecifieddepth = TauEnv_get_depth_limit();
  int mydepth = Tau_global_stackpos[tid];
  if (mydepth >= userspecifieddepth) { 
#ifndef TAU_WINDOWS
    if (TauEnv_get_ebs_enabled()) {
      Tau_sampling_resume(tid);
    }
#endif
    Tau_global_insideTAU[tid]--;
    return; 
  }
#endif /* TAU_DEPTH_LIMIT */

  p->Start(tid);

  /********************************************************************************/
  /*** Extras ***/
  /********************************************************************************/
  if (TauEnv_get_extras()) {
    /*** Memory Profiling ***/
    if (TauEnv_get_track_memory_heap()) {
      TAU_REGISTER_CONTEXT_EVENT(memHeapEvent, "Heap Memory Used (KB) at Entry");
      TAU_CONTEXT_EVENT(memHeapEvent, TauGetMaxRSS());
    }
    
    if (TauEnv_get_track_memory_headroom()) {
      TAU_REGISTER_CONTEXT_EVENT(memEvent, "Memory Headroom Available (MB) at Entry");
      TAU_CONTEXT_EVENT(memEvent, TauGetFreeMemory());
    }
    
#ifdef TAU_PROFILEMEMORY
    p->ThisFunction->GetMemoryEvent()->TriggerEvent(TauGetMaxRSS());
#endif /* TAU_PROFILEMEMORY */
    
#ifdef TAU_PROFILEHEADROOM
    p->ThisFunction->GetHeadroomEvent()->TriggerEvent((double)TauGetFreeMemory());
#endif /* TAU_PROFILEHEADROOM */
  }
  /********************************************************************************/
  /*** Extras ***/
  /********************************************************************************/

#ifndef TAU_WINDOWS
  if (TauEnv_get_ebs_enabled()) {
    Tau_sampling_resume(tid);
    Tau_sampling_event_start(tid, p->address);
  }
#endif
  Tau_global_insideTAU[tid]--;
}

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_lite_start_timer(void *functionInfo, int phase, int tid) {
  if (TauEnv_get_lite_enabled()){
    // move the stack pointer
    Tau_global_stackpos[tid]++; /* push */
    FunctionInfo *fi = (FunctionInfo *) functionInfo;
    Profiler *pp = TauInternal_ParentProfiler(tid);
    if (fi) {
      fi->IncrNumCalls(tid); // increment number of calls 
    }
    if (pp && pp->ThisFunction) {
      pp->ThisFunction->IncrNumSubrs(tid); // increment parent's child calls
    }

    
    if (Tau_global_stackpos[tid] >= Tau_global_stackdepth[tid]) {
      int oldDepth = Tau_global_stackdepth[tid];
      int newDepth = oldDepth + STACK_DEPTH_INCREMENT;
      Profiler *newStack = (Profiler *) malloc(sizeof(Profiler)*newDepth);
      memcpy(newStack, Tau_global_stack[tid], oldDepth*sizeof(Profiler));
      Tau_global_stack[tid] = newStack;
      Tau_global_stackdepth[tid] = newDepth;
    }
    Profiler *p = &(Tau_global_stack[tid][Tau_global_stackpos[tid]]);
    RtsLayer::getUSecD(tid, p->StartTime);

    p->MyProfileGroup_ = fi->GetProfileGroup();
    p->ThisFunction = fi;
    p->ParentProfiler = pp; 

    // if this function is not already on the callstack, put it
    if (fi->GetAlreadyOnStack(tid) == false) {
      p->AddInclFlag = true; 
      fi->SetAlreadyOnStack(true,tid);
    } else {
      p->AddInclFlag = false;
    }

  } else { // not lite - default 
    Tau_start_timer(functionInfo, phase, tid);
  }
}
    




///////////////////////////////////////////////////////////////////////////
static void reportOverlap (FunctionInfo *stack, FunctionInfo *caller) {
  fprintf(stderr, "[%d:%d-%d] TAU: Runtime overlap: found %s (%p) on the stack, but stop called on %s (%p)\n", 
	 RtsLayer::getPid(), RtsLayer::getTid(), RtsLayer::myThread(),
	 stack->GetName(), stack, caller->GetName(), caller);
}

///////////////////////////////////////////////////////////////////////////
extern "C" int Tau_stop_timer(void *function_info, int tid ) {
  Tau_global_insideTAU[tid]++;
  FunctionInfo *fi = (FunctionInfo *) function_info; 

  //int tid = RtsLayer::myThread();
  Profiler *profiler;

  if ( !RtsLayer::TheEnableInstrumentation() || !(fi->GetProfileGroup() & RtsLayer::TheProfileMask())) {
    Tau_global_insideTAU[tid]--;
    return 0; /* disabled */
  }

#ifndef TAU_WINDOWS
  if (TauEnv_get_ebs_enabled()) {
    Tau_sampling_suspend(tid);
  }
#endif

  /********************************************************************************/
  /*** Extras ***/
  /********************************************************************************/
  if (TauEnv_get_extras()) {
    /*** Memory Profiling ***/
    if (TauEnv_get_track_memory_heap()) {
      TAU_REGISTER_CONTEXT_EVENT(memHeapEvent, "Heap Memory Used (KB) at Exit");
      TAU_CONTEXT_EVENT(memHeapEvent, TauGetMaxRSS());
    }
    
    if (TauEnv_get_track_memory_headroom()) {
      TAU_REGISTER_CONTEXT_EVENT(memEvent, "Memory Headroom Available (MB) at Exit");
      TAU_CONTEXT_EVENT(memEvent, TauGetFreeMemory());
    }
  }
  /********************************************************************************/
  /*** Extras ***/
  /********************************************************************************/


#ifdef TAU_EPILOG
  esd_exit(fi->GetFunctionId());
#ifndef TAU_WINDOWS
    if (TauEnv_get_ebs_enabled()) {
      Tau_sampling_resume(tid);
    }
#endif
  Tau_global_insideTAU[tid]--;
  return 0;
#endif

#ifdef TAU_VAMPIRTRACE 
  uint64_t TimeStamp = vt_pform_wtime();

#ifdef TAU_VAMPIRTRACE_5_12_API
  vt_exit(VT_CURRENT_THREAD, (uint64_t *)&TimeStamp);
#else 
  vt_exit((uint64_t *)&TimeStamp);
#endif /* TAU_VAMPIRTRACE_5_12_API */

#ifndef TAU_WINDOWS
    if (TauEnv_get_ebs_enabled()) {
      Tau_sampling_resume(tid);
    }
#endif
  Tau_global_insideTAU[tid]--;
  return 0;
#endif

#ifdef TAU_SCOREP
  SCOREP_Tau_ExitRegion(fi->GetFunctionId());
#endif


  if (Tau_global_stackpos[tid] < 0) { 
#ifndef TAU_WINDOWS
    if (TauEnv_get_ebs_enabled()) {
      Tau_sampling_resume(tid);
    }
#endif
    Tau_global_insideTAU[tid]--;
    return 0; 
  }

  profiler = &(Tau_global_stack[tid][Tau_global_stackpos[tid]]);
  
  if (profiler->ThisFunction != fi) { /* Check for overlapping timers */
    reportOverlap (profiler->ThisFunction, fi);
  }


#ifdef TAU_DEPTH_LIMIT
  static int userspecifieddepth = TauEnv_get_depth_limit();
  int mydepth = Tau_global_stackpos[tid];
  if (mydepth >= userspecifieddepth) { 
    Tau_global_stackpos[tid]--; /* pop */
#ifndef TAU_WINDOWS
    if (TauEnv_get_ebs_enabled()) {
      Tau_sampling_resume(tid);
    }
#endif
    Tau_global_insideTAU[tid]--;
    return 0; 
  }
#endif /* TAU_DEPTH_LIMIT */


  profiler->Stop(tid);

  Tau_global_stackpos[tid]--; /* pop */

#ifndef TAU_WINDOWS
  if (TauEnv_get_ebs_enabled()) {
    Tau_sampling_resume(tid);
  }
#endif

  Tau_global_insideTAU[tid]--;
  return 0;
}

///////////////////////////////////////////////////////////////////////////
extern "C" int Tau_lite_stop_timer(void *function_info, int tid ) {
  if (TauEnv_get_lite_enabled()) {
    double timeStamp[TAU_MAX_COUNTERS] = {0};
    double delta [TAU_MAX_COUNTERS] = {0}; 
    RtsLayer::getUSecD(tid, timeStamp);   

    FunctionInfo *fi = (FunctionInfo *) function_info;
    Profiler *profiler;
    profiler = (Profiler *) &(Tau_global_stack[tid][Tau_global_stackpos[tid]]);

    for (int k=0; k<Tau_Global_numCounters; k++) {
      delta[k] = timeStamp[k] - profiler->StartTime[k];
    }

    if (profiler && profiler->ThisFunction != fi) { /* Check for overlapping timers */
      reportOverlap (profiler->ThisFunction, fi);
    }
    if (profiler && profiler->AddInclFlag == true) { 
      fi->SetAlreadyOnStack(false, tid); // while exiting 
      fi->AddInclTime(delta, tid); // ok to add both excl and incl times
    }
    else {
      //printf("Couldn't add incl time: profiler= %p, profiler->AddInclFlag=%d\n", profiler, profiler->AddInclFlag);
    }
    fi->AddExclTime(delta, tid); 
    Profiler *pp = TauInternal_ParentProfiler(tid); 
    
    if (pp) { 
      pp->ThisFunction->ExcludeTime(delta, tid); 
    }
    else {
      //printf("Tau_lite_stop: parent profiler = 0x0: Function name = %s, StoreData?\n", fi->GetName()); 
      TauProfiler_StoreData(tid);
    }
    Tau_global_stackpos[tid]--; /* pop */
    return 0;
  } else {
    return Tau_stop_timer(function_info, tid);
  }
}


///////////////////////////////////////////////////////////////////////////
extern "C" int Tau_stop_current_timer() {
  FunctionInfo *functionInfo;
  Profiler *profiler;
  int tid;

  tid = RtsLayer::myThread();

  if (Tau_global_stackpos[tid] < 0) return 0;

  profiler = &(Tau_global_stack[tid][Tau_global_stackpos[tid]]);
  functionInfo = profiler->ThisFunction;
  return Tau_stop_timer(functionInfo, Tau_get_tid());
}
///////////////////////////////////////////////////////////////////////////


extern "C" int Tau_profile_exit_all_tasks() {
	int tid = 1;
	while (tid < TAU_MAX_THREADS)
	{
		while (Tau_global_stackpos[tid] >= 0) {
			Profiler *p = &(Tau_global_stack[tid][Tau_global_stackpos[tid]]);
			Tau_stop_timer(p->ThisFunction, tid);
		}
	tid++;
	}
  Tau_disable_instrumentation();
  return 0;
}


extern "C" int Tau_profile_exit_all_threads() {
	int tid = 0;
	while (tid < TAU_MAX_THREADS)
	{
		while (Tau_global_stackpos[tid] >= 0) {
			Profiler *p = &(Tau_global_stack[tid][Tau_global_stackpos[tid]]);
			Tau_stop_timer(p->ThisFunction, Tau_get_tid());
			// DO NOT pop. It is popped in stop above: Tau_global_stackpos[tid]--;
		}
	tid++;
	}
  Tau_disable_instrumentation();
  return 0;
}


extern "C" int Tau_profile_exit() {
  int tid = RtsLayer::myThread();
	while (Tau_global_stackpos[tid] >= 0) {
		Profiler *p = &(Tau_global_stack[tid][Tau_global_stackpos[tid]]);
		Tau_stop_timer(p->ThisFunction, Tau_get_tid());
		// DO NOT pop. It is popped in stop above: Tau_global_stackpos[tid]--;
	}
  return 0;
}


///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_exit(const char * msg) {
  Tau_global_incr_insideTAU();

#ifdef TAU_CUDA
	Tau_profile_exit_all_threads();
#else
  Tau_profile_exit();
#endif

#ifdef TAUKTAU
  KtauProfiler::PutKtauProfiler();
#endif /* TAUKTAU */
  
#ifdef RENCI_STFF  
  RenciSTFF::cleanup();
#endif // RENCI_STFF  
  Tau_global_decr_insideTAU();
}

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_init_ref(int* argc, char ***argv) {
  RtsLayer::ProfileInit(*argc, *argv);
}

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_init(int argc, char **argv) {
  RtsLayer::ProfileInit(argc, argv);
}

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_set_node(int node) {
  if (node >= 0) TheSafeToDumpData()=1;
  RtsLayer::setMyNode(node);
}

///////////////////////////////////////////////////////////////////////////
extern "C" int Tau_get_node(void) {
  return RtsLayer::myNode();
}

///////////////////////////////////////////////////////////////////////////
extern "C" int Tau_get_context(void) {
  return RtsLayer::myContext();
}

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_set_context(int context) {
  RtsLayer::setMyContext(context);
}

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_set_thread(int thread) {
  RtsLayer::setMyThread(thread);
}

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_profile_callstack(void) {
  /* removed */
}

///////////////////////////////////////////////////////////////////////////
extern "C" int Tau_dump(void) {
  Tau_global_incr_insideTAU();
  TauProfiler_DumpData();
  Tau_global_decr_insideTAU();
  return 0;
}

///////////////////////////////////////////////////////////////////////////
extern "C" int Tau_dump_prefix(const char *prefix) {
  Tau_global_incr_insideTAU();
  TauProfiler_DumpData(false, RtsLayer::myThread(), prefix);
  Tau_global_decr_insideTAU();
  return 0;
}

///////////////////////////////////////////////////////////////////////////
extern "C" int Tau_dump_prefix_task(const char *prefix, int taskid) {
  Tau_global_incr_insideTAU();
  TauProfiler_DumpData(false, taskid, prefix);
  Tau_global_decr_insideTAU();
  return 0;
}

///////////////////////////////////////////////////////////////////////////
extern "C" int Tau_dump_incr(void) {
  Tau_global_incr_insideTAU();
  TauProfiler_DumpData(true);
  Tau_global_decr_insideTAU();
  return 0;
}

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_purge(void) {
  TauProfiler_PurgeData();
}

extern "C" void Tau_the_function_list(const char ***functionList, int *num) {
  TauProfiler_theFunctionList(functionList, num);
}

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_dump_function_names() {
  TauProfiler_dumpFunctionNames();
}
  
///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_get_counter_names(const char ***counterList, int *num) {
  TauProfiler_theCounterList(counterList, num);
}

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_get_function_values(const char **inFuncs, int numOfFuncs,
					double ***counterExclusiveValues,
					double ***counterInclusiveValues,
					int **numOfCalls, int **numOfSubRoutines,
					const char ***counterNames, int *numOfCounters) {
  TauProfiler_getFunctionValues(inFuncs,numOfFuncs,counterExclusiveValues,counterInclusiveValues,
				numOfCalls,numOfSubRoutines,counterNames,numOfCounters);
}


///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_get_event_names(const char ***eventList, int *num) {
  TauProfiler_getUserEventList(eventList, num);
}


///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_get_event_vals(const char **inUserEvents, int numUserEvents,
				  int **numEvents, double **max, double **min,
				  double **mean, double **sumSqr) {
  TauProfiler_getUserEventValues(inUserEvents, numUserEvents, numEvents, max, min,
				 mean, sumSqr);
}



///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_dump_function_values(const char **functionList, int num) {
  TauProfiler_dumpFunctionValues(functionList,num);
}


///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_dump_function_values_incr(const char **functionList, int num) {
  TauProfiler_dumpFunctionValues(functionList,num,true);
}


///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_register_thread(void) {
  RtsLayer::RegisterThread();
}


///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_register_fork(int nodeid, enum TauFork_t opcode) {
  RtsLayer::RegisterFork(nodeid, opcode);
}


///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_enable_instrumentation(void) {
  RtsLayer::TheEnableInstrumentation() = true;
}


///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_disable_instrumentation(void) {
  RtsLayer::TheEnableInstrumentation() = false;
}


///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_shutdown(void) {
  if (!TheUsingCompInst()) {
    RtsLayer::TheShutdown() = true;
    RtsLayer::TheEnableInstrumentation() = false;
  }
}


///////////////////////////////////////////////////////////////////////////
extern "C" TauGroup_t Tau_enable_group_name(char * group) {
  return RtsLayer::enableProfileGroupName(group);
}


///////////////////////////////////////////////////////////////////////////
extern "C" TauGroup_t Tau_disable_group_name(char * group) {
  return RtsLayer::disableProfileGroupName(group);
}


///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_profile_set_group_name(void *ptr, const char *groupname) {
  FunctionInfo *f = (FunctionInfo*)ptr;
  f->SetPrimaryGroupName(groupname);
}

extern "C" void Tau_profile_set_name(void *ptr, const char *name) {
  FunctionInfo *f = (FunctionInfo*)ptr;
  f->Name = strdup(name);
}

extern "C" void Tau_profile_set_type(void *ptr, const char *type) {
  FunctionInfo *f = (FunctionInfo*)ptr;
  f->Type = strdup(type);
}

extern "C" void Tau_profile_set_group(void *ptr, TauGroup_t group) {
  FunctionInfo *f = (FunctionInfo*)ptr;
  f->SetProfileGroup(group);
}

extern "C" const char *Tau_profile_get_group_name(void *ptr) {
  FunctionInfo *f = (FunctionInfo*)ptr;
  return f->GroupName;
}

extern "C" const char *Tau_profile_get_name(void *ptr) {
  FunctionInfo *f = (FunctionInfo*)ptr;
  return f->Name;
}

extern "C" const char *Tau_profile_get_type(void *ptr) {
  FunctionInfo *f = (FunctionInfo*)ptr;
  return f->Type;
}

extern "C" TauGroup_t Tau_profile_get_group(void *ptr) {
  FunctionInfo *f = (FunctionInfo*)ptr;
  return f->GetProfileGroup();
}

///////////////////////////////////////////////////////////////////////////
extern "C" TauGroup_t Tau_get_profile_group(char * group) {
  return RtsLayer::getProfileGroup(group);
}


///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_enable_group(TauGroup_t group) {
  RtsLayer::enableProfileGroup(group);
}


///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_disable_group(TauGroup_t group) {
  RtsLayer::disableProfileGroup(group);
}


///////////////////////////////////////////////////////////////////////////
extern "C" TauGroup_t Tau_enable_all_groups(void) {
  return RtsLayer::enableAllGroups();
}


///////////////////////////////////////////////////////////////////////////
extern "C" TauGroup_t Tau_disable_all_groups(void) {
  return RtsLayer::disableAllGroups();
}



///////////////////////////////////////////////////////////////////////////
extern "C" int& tau_totalnodes(int set_or_get, int value)
{
  static int nodes = 1;
  if (set_or_get == 1) {
    nodes = value;
  }
  return nodes;
}



#if (defined(TAU_MPI) || defined(TAU_SHMEM) || defined(TAU_DMAPP) || defined(TAU_UPC) || defined(TAU_GPI) )



#ifdef TAU_SYSTEMWIDE_TRACK_MSG_SIZE_AS_CTX_EVENT 
#define TAU_GEN_EVENT(e, msg) TauContextUserEvent* e () { \
        static TauContextUserEvent ce(msg); return &ce; }
#undef TAU_EVENT
#define TAU_EVENT(event,data) Tau_context_userevent(event, data);
#else
#define TAU_GEN_EVENT(e, msg) TauUserEvent* e () { \
	static TauUserEvent u(msg); return &u; } 
#endif /* TAU_SYSTEMWIDE_TRACK_MSG_SIZE_AS_CTX_EVENT */


#define TAU_GEN_CONTEXT_EVENT(e, msg) TauContextUserEvent* e () { \
	static TauContextUserEvent ce(msg); return &ce; } 

TAU_GEN_EVENT(TheSendEvent,"Message size sent to all nodes")
TAU_GEN_EVENT(TheRecvEvent,"Message size received from all nodes")
TAU_GEN_EVENT(TheBcastEvent,"Message size for broadcast")
TAU_GEN_EVENT(TheReduceEvent,"Message size for reduce")
TAU_GEN_EVENT(TheReduceScatterEvent,"Message size for reduce-scatter")
TAU_GEN_EVENT(TheScanEvent,"Message size for scan")
TAU_GEN_EVENT(TheAllReduceEvent,"Message size for all-reduce")
TAU_GEN_EVENT(TheAlltoallEvent,"Message size for all-to-all")
TAU_GEN_EVENT(TheScatterEvent,"Message size for scatter")
TAU_GEN_EVENT(TheGatherEvent,"Message size for gather")
TAU_GEN_EVENT(TheAllgatherEvent,"Message size for all-gather")
TAU_GEN_CONTEXT_EVENT(TheWaitEvent,"Message size received in wait")

TauContextUserEvent & TheMsgVolSendContextEvent(int tid) {
    static TauContextUserEvent ** sendEvents = NULL;

    if(!sendEvents) {
        sendEvents = (TauContextUserEvent**)calloc(tau_totalnodes(0,0), sizeof(TauContextUserEvent*));
    }

    if(!sendEvents[tid]) {
        char buff[256];
        sprintf(buff, "Message size sent to node %d", tid);
        sendEvents[tid] = new TauContextUserEvent(buff);
    }

    return *(sendEvents[tid]);
}

TauContextUserEvent & TheMsgVolRecvContextEvent(int tid) {
    static TauContextUserEvent ** recvEvents = NULL;

    if(!recvEvents) {
        recvEvents = (TauContextUserEvent**)calloc(tau_totalnodes(0,0), sizeof(TauContextUserEvent*));
    }

    if(!recvEvents[tid]) {
        char buff[256];
        sprintf(buff, "Message size received from node %d", tid);
        recvEvents[tid] = new TauContextUserEvent(buff);
    }

    return *(recvEvents[tid]);
}

///////////////////////////////////////////////////////////////////////////
#ifdef TAU_SHMEM 
extern "C" int shmem_n_pes(void); 
#endif /* TAU_SHMEM */

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_trace_sendmsg(int type, int destination, int length) 
{
  if (!RtsLayer::TheEnableInstrumentation()) return; 

#ifdef TAU_PROFILEPARAM
#ifndef TAU_DISABLE_PROFILEPARAM_IN_MPI
  TAU_PROFILE_PARAM1L(length, "message size");
#endif /* TAU_DISABLE_PROFILEPARAM_IN_MPI */
#endif  /* TAU_PROFILEPARAM */

  TAU_EVENT(TheSendEvent(), length);

  if (TauEnv_get_comm_matrix()) {
    if (destination >= tau_totalnodes(0,0)) {
#ifdef TAU_SHMEM
      tau_totalnodes(1,shmem_n_pes());
#else /* TAU_SHMEM */
      fprintf(stderr, 
          "TAU Error: Comm Matrix destination %d exceeds node count %d. "
          "Was MPI_Init/shmem_init wrapper never called? "
          "Please disable TAU_COMM_MATRIX or add calls to the init function in your source code.\n", 
          destination, tau_totalnodes(0,0));
      exit(-1);
#endif /* TAU_SHMEM */
    }
    TheMsgVolSendContextEvent(destination).TriggerEvent(length, RtsLayer::myThread());
  }

  if (TauEnv_get_tracing()) {
    if (destination >= 0) {
      TauTraceSendMsg(type, destination, length);
    }
  }
}

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_trace_recvmsg(int type, int source, int length) {

#ifdef TAU_PROFILEPARAM
#ifndef TAU_DISABLE_PROFILEPARAM_IN_MPI
  TAU_PROFILE_PARAM1L(length, "message size");
#endif /* TAU_DISABLE_PROFILEPARAM_IN_MPI */
#endif  /* TAU_PROFILEPARAM */

  TAU_EVENT(TheRecvEvent(), length);

  if (TauEnv_get_tracing()) {
    if (source >= 0) {
      TauTraceRecvMsg(type, source, length);
    }
  }
}

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_trace_recvmsg_remote(int type, int source, int length, int remoteid) 
{
  if (!RtsLayer::TheEnableInstrumentation()) return; 
  if (TauEnv_get_tracing()) {
    if (source >= 0) {
      TauTraceRecvMsgRemote(type, source, length, remoteid);
    }
  }
}

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_trace_sendmsg_remote(int type, int destination, int length, int remoteid) 
{
  if (!RtsLayer::TheEnableInstrumentation()) return; 

  if (TauEnv_get_tracing()) {
    if (destination >= 0) {
      TauTraceSendMsgRemote(type, destination, length, remoteid);
    }
  }

  if (TauEnv_get_comm_matrix())  {

#ifdef TAU_PROFILEPARAM
#ifndef TAU_DISABLE_PROFILEPARAM_IN_MPI
    TAU_PROFILE_PARAM1L(length, "message size");
#endif /* TAU_DISABLE_PROFILEPARAM_IN_MPI */
#endif  /* TAU_PROFILEPARAM */

    if (TauEnv_get_comm_matrix()) {
      if (destination >= tau_totalnodes(0,0)) {
#ifdef TAU_SHMEM
        tau_totalnodes(1,shmem_n_pes());
#else /* TAU_SHMEM */
        fprintf(stderr, 
            "TAU Error: Comm Matrix destination %d exceeds node count %d. "
            "Was MPI_Init/shmem_init wrapper never called? "
            "Please disable TAU_COMM_MATRIX or add calls to the init function in your source code.\n", 
            destination, tau_totalnodes(0,0));
        exit(-1);
#endif /* TAU_SHMEM */
      }
      TheMsgVolRecvContextEvent(remoteid).TriggerEvent(length, RtsLayer::myThread());
    }

  }
}


extern "C" void Tau_bcast_data(int data) {
  TAU_EVENT(TheBcastEvent(), data);
}

extern "C" void Tau_reduce_data(int data) {
  TAU_EVENT(TheReduceEvent(), data);
}

extern "C" void Tau_alltoall_data(int data) {
  TAU_EVENT(TheAlltoallEvent(), data);
}

extern "C" void Tau_scatter_data(int data) {
  TAU_EVENT(TheScatterEvent(), data);
}

extern "C" void Tau_gather_data(int data) {
  TAU_EVENT(TheGatherEvent(), data);
}

extern "C" void Tau_allgather_data(int data) {
  TAU_EVENT(TheAllgatherEvent(), data);
}

extern "C" void Tau_wait_data(int data) {
  TAU_CONTEXT_EVENT(TheWaitEvent(), data);
}

extern "C" void Tau_allreduce_data(int data) {
  TAU_EVENT(TheAllReduceEvent(), data);
}

extern "C" void Tau_scan_data(int data) {
  TAU_EVENT(TheScanEvent(), data);
}

extern "C" void Tau_reducescatter_data(int data) {
  TAU_EVENT(TheReduceScatterEvent(), data);
}

#else /* !(TAU_MPI || TAU_SHMEM || TAU_DMAPP || TAU_GPI)*/

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_trace_sendmsg(int type, int destination, int length) {
  TauTraceSendMsg(type, destination, length);
}
///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_trace_sendmsg_remote(int type, int destination, int length, int remoteid) {
  TauTraceSendMsgRemote(type, destination, length, remoteid);
}
///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_trace_recvmsg(int type, int source, int length) {
  TauTraceRecvMsg(type, source, length);
}
///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_trace_recvmsg_remote(int type, int source, int length, int remoteid) {
  TauTraceRecvMsgRemote(type, source, length, remoteid);
}

#endif /* TAU_MPI || TAU_SHMEM*/

///////////////////////////////////////////////////////////////////////////
// User Defined Events 
///////////////////////////////////////////////////////////////////////////
extern "C" void * Tau_get_userevent(char *name) {
  TauUserEvent *ue;
  ue = new TauUserEvent(name);
  return (void *) ue;
}

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_userevent(void *ue, double data) {
  TauUserEvent *t = (TauUserEvent *) ue;
  t->TriggerEvent(data);
} 

extern "C" void Tau_userevent_thread(void *ue, double data, int tid) {
  TauUserEvent *t = (TauUserEvent *) ue;
  t->TriggerEvent(data, tid);
}

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_get_context_userevent(void **ptr, char *name)
{
  if (*ptr == 0) {
    RtsLayer::LockEnv();

    if (*ptr == 0) {
      TauContextUserEvent *ue;
      ue = new TauContextUserEvent(name);
      *ptr = (void*) ue;
    }

    RtsLayer::UnLockEnv();
  }
  return;
}

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_context_userevent(void *ue, double data) {
  TauContextUserEvent *t = (TauContextUserEvent *) ue;
  t->TriggerEvent(data);
} 

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_context_userevent_thread(void *ue, double data, int tid) {
  TauContextUserEvent *t = (TauContextUserEvent *) ue;
  t->TriggerEvent(data, tid);
}

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_set_event_name(void *ue, char *name) {
  TauUserEvent *t = (TauUserEvent *) ue;
  t->SetEventName(name);
}

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_report_statistics(void) {
  TauUserEvent::ReportStatistics();
}

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_report_thread_statistics(void) {
  TauUserEvent::ReportStatistics(true);
}

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_event_disable_min(void *ue) {
  TauUserEvent *t = (TauUserEvent *) ue;
  t->SetDisableMin(true);
} 

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_event_disable_max(void *ue) {
  TauUserEvent *t = (TauUserEvent *) ue;
  t->SetDisableMax(true);
} 

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_event_disable_mean(void *ue) {
  TauUserEvent *t = (TauUserEvent *) ue;
  t->SetDisableMean(true);
} 

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_event_disable_stddev(void *ue) {
  TauUserEvent *t = (TauUserEvent *) ue;
  t->SetDisableStdDev(true);
} 

///////////////////////////////////////////////////////////////////////////

extern "C" void Tau_profile_c_timer(void **ptr, const char *name, const char *type, 
    TauGroup_t group, const char *group_name) 
{
  if (*ptr == 0) {
    RtsLayer::LockEnv();
    if (*ptr == 0) {  
      Tau_global_incr_insideTAU();
      // remove garbage characters from the end of name
      unsigned int len=0;
      while(isprint(name[len])) {
        ++len;
      }

      char * fixedname = (char*)malloc(len+1);
      memcpy(fixedname, name, len);
      fixedname[len] = '\0';

      *ptr = Tau_get_profiler(fixedname, type, group, group_name);

      free((void*)fixedname);
      Tau_global_decr_insideTAU();
    }
    RtsLayer::UnLockEnv();
  }
}

///////////////////////////////////////////////////////////////////////////

//static string gTauApplication = string(".TAU application");

static string& gTauApplication()
{
	static string g = string(".TAU application");
	return g;

}

extern void Tau_pure_start_task_string(const string name, int tid);

/* We need a routine that will create a top level parent profiler and give
 * it a dummy name for the application, if just the MPI wrapper interposition
 * library is used without any instrumentation in main */
extern "C" void Tau_create_top_level_timer_if_necessary_task(int tid) {

/*
  int disabled = 0;
#ifdef TAU_VAMPIRTRACE
  disabled = 1;
#endif
#ifdef TAU_EPILOG
  disabled = 1;
#endif
  if (disabled) {
    return;
  }
*/
#if defined(TAU_VAMPIRTRACE) || defined(TAU_EPILOG)
	return // disabled.
#endif
  /* After creating the ".TAU application" timer, we start it. In the
     timer start code, it will call this function, so in that case,
  	 return right away. */
  static bool initialized = false;
  static bool initthread[TAU_MAX_THREADS] = {false};
  static bool initializing[TAU_MAX_THREADS] = {false};

  if (!initialized) {
    if (initializing[tid]) {
      return;
    }
    RtsLayer::LockEnv();
    if (!initialized) {
	  // whichever thread got here first, has the lock and will create the
	  // FunctionInfo object for the top level timer.
      if (TauInternal_CurrentProfiler(tid) == NULL) {
        initthread[tid] = true;
		initializing[tid] = true;
        Tau_pure_start_task_string(gTauApplication(), tid);
		initializing[tid] = false;
        initialized = true;
      }
    }
    RtsLayer::UnLockEnv();
  }

  if (initthread[tid] == true) {
    return;
  }
  
  // if there is no top-level timer, create one - But only create one FunctionInfo object.
  // that should be handled by the Tau_pure_start_task call.
  if (TauInternal_CurrentProfiler(tid) == NULL) {
    initthread[tid] = true;
    initializing[tid] = true;
    Tau_pure_start_task_string(gTauApplication(), tid);
    initializing[tid] = false;
  }

  atexit(Tau_destructor_trigger);
}

extern "C" void Tau_create_top_level_timer_if_necessary(void) {
  return Tau_create_top_level_timer_if_necessary_task(Tau_get_tid());
}


extern "C" void Tau_stop_top_level_timer_if_necessary_task(int tid) {
  if (TauInternal_CurrentProfiler(tid) && 
      TauInternal_CurrentProfiler(tid)->ParentProfiler == NULL && 
      strcmp(TauInternal_CurrentProfiler(tid)->ThisFunction->GetName(), ".TAU application") == 0) {
    DEBUGPROFMSG("Found top level .TAU application timer"<<endl;);  
    TAU_GLOBAL_TIMER_STOP();
  }
}

extern "C" void Tau_stop_top_level_timer_if_necessary(void) {
   Tau_stop_top_level_timer_if_necessary_task(RtsLayer::myThread());
}


extern "C" void Tau_disable_context_event(void *event) {
  TauContextUserEvent *e = (TauContextUserEvent *) event;
  e->SetDisableContext(true);
}

extern "C" void Tau_enable_context_event(void *event) {
  TauContextUserEvent *e = (TauContextUserEvent *) event;
  e->SetDisableContext(false);
}



extern "C" void Tau_track_memory(void) {
  TauTrackMemoryUtilization(true);
}


extern "C" void Tau_track_memory_here(void) {
  TauTrackMemoryHere();
}


extern "C" void Tau_track_memory_headroom(void) {
  TauTrackMemoryUtilization(false);
}


extern "C" void Tau_track_memory_headroom_here(void) {
  TauTrackMemoryHeadroomHere();
}


extern "C" void Tau_enable_tracking_memory(void) {
  TauEnableTrackingMemory();
}


extern "C" void Tau_disable_tracking_memory(void) {
  TauDisableTrackingMemory();
}


extern "C" void Tau_enable_tracking_memory_headroom(void) {
  TauEnableTrackingMemoryHeadroom();
}


extern "C" void Tau_disable_tracking_memory_headroom(void) {
  TauDisableTrackingMemoryHeadroom();
}

extern "C" void Tau_set_interrupt_interval(int value) {
  TauSetInterruptInterval(value);
}


extern "C" void Tau_global_stop(void) {
  Tau_stop_current_timer();
}

///////////////////////////////////////////////////////////////////////////
extern "C" char * Tau_phase_enable(const char *group) {
#ifdef TAU_PROFILEPHASE
  char *newgroup = new char[strlen(group)+16];
  sprintf(newgroup, "%s | TAU_PHASE", group);
  return newgroup;
#else /* TAU_PROFILEPHASE */
  return (char *) group;
#endif /* TAU_PROFILEPHASE */
} 

///////////////////////////////////////////////////////////////////////////
extern "C" char * Tau_phase_enable_once(const char *group, void **ptr) {
  /* We don't want to parse the group name string every time this is invoked.
     we compare a pointer and if necessary, perform the string operations  
     on the group name (adding  | TAU_PHASE to it). */
  if (*ptr == 0) {
    return Tau_phase_enable(group);
  } else {
    return (char *) NULL;
  }
} 

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_mark_group_as_phase(void *ptr) {
  FunctionInfo *fptr = (FunctionInfo *) ptr;
  char *newgroup = Tau_phase_enable(fptr->GetAllGroups()); 
  fptr->SetPrimaryGroupName(newgroup); 
}

///////////////////////////////////////////////////////////////////////////
extern "C" char * Tau_append_iteration_to_name(int iteration, char *name) {
  char tau_iteration_number[128];
  sprintf(tau_iteration_number, " [%d]", iteration);
  string iterationName = string(name)+string(tau_iteration_number);
  char *newName = strdup(iterationName.c_str());
  return newName;
}

///////////////////////////////////////////////////////////////////////////

extern "C" void Tau_profile_dynamic_auto(int iteration, void **ptr, char *fname, char *type, TauGroup_t group, char *group_name, int isPhase)
{ /* This routine creates dynamic timers and phases by embedding the
     iteration number in the name. isPhase argument tells whether we
     choose phases or timers. */

  char *newName = Tau_append_iteration_to_name(iteration, fname);

  /* create the pointer. */
  Tau_profile_c_timer(ptr, newName, type, group, group_name);

  /* annotate it as a phase if it is */
  if (isPhase)
    Tau_mark_group_as_phase(ptr);
  free(newName);

}

///////////////////////////////////////////////////////////////////////////
extern "C" void Tau_profile_param1l(long data, const char *dataname) {
  string dname(dataname);
#ifdef TAU_PROFILEPARAM
  TauProfiler_AddProfileParamData(data, dataname);
#ifdef TAU_SCOREP
  SCOREP_Tau_ParamHandle handle = SCOREP_TAU_INIT_PARAM_HANDLE;
  SCOREP_Tau_Parameter_INT64(&handle, dataname, data);
#endif
#endif
}


/*
  The following is for supporting pure and elemental fortran subroutines
*/

TAU_HASH_MAP<string, FunctionInfo *>& ThePureMap() {
  static TAU_HASH_MAP<string, FunctionInfo *> pureMap;
  return pureMap;
}

map<string, int *>& TheIterationMap() {
  static map<string, int *> iterationMap;
  return iterationMap;
}

void Tau_pure_start_task_string(const string name, int tid)
{
  FunctionInfo *fi = 0;
  RtsLayer::LockDB();
  TAU_HASH_MAP<string, FunctionInfo *>::iterator it = ThePureMap().find(name);
  if (it == ThePureMap().end()) {
    tauCreateFI((void**)&fi,name,"",TAU_USER,"TAU_USER");
    ThePureMap()[name] = fi;
  } else {
    fi = (*it).second;
  }
  RtsLayer::UnLockDB();
  Tau_start_timer(fi,0, tid);
}

extern "C" void Tau_pure_start_task(const char *name, int tid)
{
  Tau_global_incr_insideTAU_tid(tid);
  string n = string(name);
  Tau_pure_start_task_string(n, tid);
  Tau_global_decr_insideTAU_tid(tid);
}

extern "C" void Tau_pure_start(const char *name) {
  int tid = Tau_get_tid();
  Tau_global_incr_insideTAU_tid(tid);
  string n = string(name);
  Tau_pure_start_task_string(n, tid);
  Tau_global_decr_insideTAU_tid(tid);
}

void Tau_pure_stop_task_string(const string name, int tid) {
  FunctionInfo *fi;
  // Locking the access to the FunctionInfo map
  RtsLayer::LockDB();
  TAU_HASH_MAP<string, FunctionInfo *>::iterator it = ThePureMap().find(name);
  if (it == ThePureMap().end()) {
    fprintf (stderr, "\nTAU Error: Routine \"%s\" does not exist, did you misspell it with TAU_STOP()?\nTAU Error: You will likely get an overlapping timer message next\n\n", name.c_str());
  } else {
    fi = (*it).second;
  }
  // releasing the FunctionInfo map
  RtsLayer::UnLockDB();
  Tau_stop_timer(fi, tid);
}

extern "C" void Tau_pure_stop_task(const char *name, int tid) {
  Tau_global_incr_insideTAU_tid(tid);
  string n = string(name);
  Tau_pure_stop_task_string(n, tid);
  Tau_global_decr_insideTAU_tid(tid);
}

extern "C" void Tau_pure_stop(const char *name) {
  int tid = Tau_get_tid();
  Tau_global_incr_insideTAU_tid(tid);
  string n = string(name);
  Tau_pure_stop_task_string(n, Tau_get_tid());
  Tau_global_decr_insideTAU_tid(tid);
}

extern "C" void Tau_static_phase_start(char *name) {

//printf("Static phase: %s\n", name);
  FunctionInfo *fi = 0;
  string n = string(name);
  RtsLayer::LockDB();
  TAU_HASH_MAP<string, FunctionInfo *>::iterator it = ThePureMap().find(n);
  if (it == ThePureMap().end()) {
    tauCreateFI((void**)&fi,n,"",TAU_USER,"TAU_USER");
    Tau_mark_group_as_phase(fi);
    ThePureMap()[n] = fi;
  } else {
    fi = (*it).second;
  }   
  RtsLayer::UnLockDB();
  Tau_start_timer(fi,1, Tau_get_tid());
}

extern "C" void Tau_static_phase_stop(char *name) {
  FunctionInfo *fi;
  string n = string(name);
  RtsLayer::LockDB();
  TAU_HASH_MAP<string, FunctionInfo *>::iterator it = ThePureMap().find(n);
  if (it == ThePureMap().end()) {
    fprintf (stderr, "\nTAU Error: Routine \"%s\" does not exist, did you misspell it with TAU_STOP()?\nTAU Error: You will likely get an overlapping timer message next\n\n", name);
  } else {
    fi = (*it).second;
  }
  RtsLayer::UnLockDB();
  Tau_stop_timer(fi, Tau_get_tid());
}


static int *getIterationList(char *name) {
  string searchName(name);
  map<string, int *>::iterator iit = TheIterationMap().find(searchName);
  if (iit == TheIterationMap().end()) {
    RtsLayer::LockEnv();
    int *iterationList = new int[TAU_MAX_THREADS];
    for (int i=0; i<TAU_MAX_THREADS; i++) {
      iterationList[i] = 0;
    }
    TheIterationMap()[searchName] = iterationList;
    RtsLayer::UnLockEnv();
  }
  return TheIterationMap()[searchName];
}

/* isPhase argument is 1 for phase and 0 for timer */
extern "C" void Tau_dynamic_start(char *name, int isPhase) {
#ifndef TAU_PROFILEPHASE
  isPhase = 0;
#endif

  int *iterationList = getIterationList(name);

  int tid = RtsLayer::myThread();
  int itcount = iterationList[tid];

  FunctionInfo *fi = NULL;
  char *newName = Tau_append_iteration_to_name(itcount, name);
  string n (newName);
  free(newName);
  
  RtsLayer::LockDB();
  TAU_HASH_MAP<string, FunctionInfo *>::iterator it = ThePureMap().find(n);
  if (it == ThePureMap().end()) {
    tauCreateFI((void**)&fi,n,"",TAU_USER,"TAU_USER");
    if (isPhase) {
      Tau_mark_group_as_phase(fi);
    }
    ThePureMap()[n] = fi;
  } else {
    fi = (*it).second;
  }   
  RtsLayer::UnLockDB();
  Tau_start_timer(fi,isPhase, Tau_get_tid());
}


/* isPhase argument is ignored in Tau_dynamic_stop. For consistency with
   Tau_dynamic_start. */
extern "C" void Tau_dynamic_stop(char *name, int isPhase) {
  
  int *iterationList = getIterationList(name);

  int tid = RtsLayer::myThread();
  int itcount = iterationList[tid];

  // increment the counter
  iterationList[tid]++;
  
  FunctionInfo *fi = NULL;   
  char *newName = Tau_append_iteration_to_name(itcount, name);
  string n (newName);
  free(newName);
  RtsLayer::LockDB();
  TAU_HASH_MAP<string, FunctionInfo *>::iterator it = ThePureMap().find(n);
  if (it == ThePureMap().end()) {
    fprintf (stderr, "\nTAU Error: Routine \"%s\" does not exist, did you misspell it with TAU_STOP()?\nTAU Error: You will likely get an overlapping timer message next\n\n", name);
    RtsLayer::UnLockDB();
    return;
  } else {
    fi = (*it).second;
  }
  RtsLayer::UnLockDB();
  Tau_stop_timer(fi, Tau_get_tid());
}


#if (!defined(TAU_WINDOWS))
extern "C" pid_t tau_fork() {
  pid_t pid;

  pid = fork();

#ifdef TAU_WRAP_FORK
  if (pid == 0) {
    TAU_REGISTER_FORK(getpid(), TAU_EXCLUDE_PARENT_DATA);
//     fprintf (stderr, "[%d] Registered Fork!\n", getpid());
  } else {
    /* nothing */
  }
#endif

  return pid;
}
#endif /* TAU_WINDOWS */


//////////////////////////////////////////////////////////////////////
// Snapshot related routines
//////////////////////////////////////////////////////////////////////

extern "C" void Tau_profile_snapshot_1l(const char *name, int number) {
  char buffer[4096];
  sprintf (buffer, "%s %d", name, number);
  Tau_snapshot_writeIntermediate(buffer);
}

extern "C" void Tau_profile_snapshot(const char *name) {
  Tau_snapshot_writeIntermediate(name);
}


static int Tau_usesMPI = 0;
extern "C" void Tau_set_usesMPI(int value) {
  Tau_usesMPI = value;
}

extern "C" int Tau_get_usesMPI() {
  return Tau_usesMPI;
}

//////////////////////////////////////////////////////////////////////
extern "C" void Tau_get_calls(void *handle, long *values, int tid) {
  FunctionInfo *ptr = (FunctionInfo *)handle;

  values[0] = (long) ptr->GetCalls(tid);
  return;
}

//////////////////////////////////////////////////////////////////////
extern "C" void Tau_set_calls(void *handle, long values, int tid) {
  FunctionInfo *ptr = (FunctionInfo *)handle;

  ptr->SetCalls(tid, values);
  return;
}

//////////////////////////////////////////////////////////////////////
void Tau_get_child_calls(void *handle, long* values, int tid) {
  FunctionInfo *ptr = (FunctionInfo *)handle;

  values[0] = (long) ptr->GetSubrs(tid);
  return;
}

//////////////////////////////////////////////////////////////////////
extern "C" void Tau_set_child_calls(void *handle, long values, int tid) {
  FunctionInfo *ptr = (FunctionInfo *)handle;

  ptr->SetSubrs(tid, values);
  return;
}

//////////////////////////////////////////////////////////////////////
extern "C" void Tau_get_inclusive_values(void *handle, double* values, int tid) {
  FunctionInfo *ptr = (FunctionInfo *)handle;
  
  if (ptr)
    ptr->getInclusiveValues(tid, values);
  return;
}

//////////////////////////////////////////////////////////////////////
extern "C" void Tau_set_inclusive_values(void *handle, double* values, int tid) {
  FunctionInfo *ptr = (FunctionInfo *)handle;
  
  if (ptr) {
    ptr->SetInclTime(tid, values);
  }
}

//////////////////////////////////////////////////////////////////////
extern "C" void Tau_get_exclusive_values(void *handle, double* values, int tid) {
  FunctionInfo *ptr = (FunctionInfo *)handle;
 
  if (ptr) {
    ptr->getExclusiveValues(tid, values);
  }
  return;
}

//////////////////////////////////////////////////////////////////////
extern "C" void Tau_set_exclusive_values(void *handle, double* values, int tid) {
  FunctionInfo *ptr = (FunctionInfo *)handle;
  
  if (ptr) {
    ptr->SetExclTime(tid, values);
  }
  return;
}

//////////////////////////////////////////////////////////////////////
extern "C" void Tau_get_counter_info(const char ***counterNames, int *numCounters) {
  TauMetrics_getCounterList(counterNames, numCounters);
}

//////////////////////////////////////////////////////////////////////
extern "C" int Tau_get_tid(void) {
  return RtsLayer::myThread();
}

extern "C" int Tau_create_tid(void) {
  return RtsLayer::threadId();
}

// this routine is called by the destructors of our static objects
// ensuring that the profiles are written out while the objects are still valid
void Tau_destructor_trigger() {
  Tau_stop_top_level_timer_if_necessary();
  Tau_global_setLightsOut();
  if ((TheUsingDyninst() || TheUsingCompInst()) && TheSafeToDumpData()) {
#ifndef TAU_VAMPIRTRACE
    TAU_PROFILE_EXIT("FunctionDB destructor");
    TheSafeToDumpData() = 0;
#endif
  }
}

//////////////////////////////////////////////////////////////////////
extern "C" int Tau_create_task(void) {
  int taskid;
  if (TAU_MAX_THREADS == 1) {
    printf("TAU: ERROR: Please re-configure TAU with -useropt=-DTAU_MAX_THREADS=100  and rebuild it to use the new TASK API\n");
  }
  taskid= RtsLayer::RegisterThread() - 1; /* it returns 1 .. N, we want 0 .. N-1 */
  /* specify taskid is a fake thread used in the Task API */
  Tau_is_thread_fake_for_task_api[taskid] = 1; /* This thread is fake! */
 	//printf("create task with id: %d.\n", taskid); 
  return taskid;
}



/**************************************************************************
 Query API allowing a program/library to query the TAU callstack
***************************************************************************/
void *Tau_query_current_event() {
  Profiler *profiler = TauInternal_CurrentProfiler(RtsLayer::myThread());
  return (void*)profiler;
}

const char *Tau_query_event_name(void *event) {
  if (event == NULL) {
    return NULL;
  }
  Profiler *profiler = (Profiler*) event;
  return profiler->ThisFunction->Name;
}

void *Tau_query_parent_event(void *event) {
  //Profiler *profiler = (Profiler*) event;
  int tid = RtsLayer::myThread();
  void *topOfStack = &(Tau_global_stack[tid][0]);
  if (event == topOfStack) {
    return NULL;
  } else {
    long loc = Tau_convert_ptr_to_long(event);
    return (void*)(loc - (sizeof(Profiler)));
  }
}



//////////////////////////////////////////////////////////////////////
// User definable clock
//////////////////////////////////////////////////////////////////////
extern "C" void Tau_set_user_clock(double value) {
  int tid = RtsLayer::myThread();
  metric_write_userClock(tid, value);
}

extern "C" void Tau_set_user_clock_thread(double value, int tid) {
  metric_write_userClock(tid, value);
}

extern "C" long Tau_convert_ptr_to_long(void *ptr) {
  long long a = (long long) ptr;
  long ret = (long) a;
  return ret;
}

extern "C" unsigned long Tau_convert_ptr_to_unsigned_long(void *ptr) {
  unsigned long long a = (unsigned long long) ptr;
  unsigned long ret = (unsigned long) a;
  return ret;
}

//////////////////////////////////////////////////////////////////////
// Sometimes we may link in a library that needs the POMP stuff
// Even when we're not using opari
//////////////////////////////////////////////////////////////////////

#ifdef TAU_SICORTEX
#define TAU_FALSE_POMP
#endif

#ifdef TAU_FALSE_POMP

#pragma weak POMP_MAX_ID=TAU_POMP_MAX_ID
int TAU_POMP_MAX_ID = 0;
#pragma weak pomp_rd_table=tau_pomp_rd_table
int *tau_pomp_rd_table = 0;

// #pragma weak omp_get_thread_num=tau_omp_get_thread_num
// extern "C" int tau_omp_get_thread_num() {
//   return 0;
// }

// #ifdef TAU_OPENMP
// extern "C" {
//   void pomp_parallel_begin();
//   void pomp_parallel_fork_();
//   void POMP_Parallel_fork();
//   void _tau_pomp_will_not_be_called() {
//     pomp_parallel_begin();
//     pomp_parallel_fork_();
//     POMP_Parallel_fork();
//   }
// }
// #endif
#endif

#ifndef TAU_BGP
extern "C" void Tau_Bg_hwp_counters_start(int *error) {
}

extern "C" void Tau_Bg_hwp_counters_stop(int* numCounters, x_uint64 counters[], int* mode, int *error) {
}

extern "C" void Tau_Bg_hwp_counters_output(int* numCounters, x_uint64 counters[], int* mode, int* error) {
}
#endif /* TAU_BGP */
                    

/***************************************************************************
 * $RCSfile: TauCAPI.cpp,v $   $Author: sameer $
 * $Revision: 1.158 $   $Date: 2010/05/28 17:45:49 $
 * VERSION: $Id: TauCAPI.cpp,v 1.158 2010/05/28 17:45:49 sameer Exp $
 ***************************************************************************/

