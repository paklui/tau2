/****************************************************************************
**			TAU Portable Profiling Package			   **
**			http://www.cs.uoregon.edu/research/tau	           **
*****************************************************************************
**    Copyright 2008  						   	   **
**    Department of Computer and Information Science, University of Oregon **
**    Advanced Computing Laboratory, Los Alamos National Laboratory        **
**    Forschungszentrum Juelich                                            **
****************************************************************************/
/****************************************************************************
**	File 		: TauInit.cpp 			        	   **
**	Description 	: TAU Profiling Package				   **
**	Author		: Alan Morris					   **
**	Contact		: tau-bugs@cs.uoregon.edu               	   **
**	Documentation	: See http://www.cs.uoregon.edu/research/tau       **
**                                                                         **
**      Description     : TAU initialization                               **
**                                                                         **
****************************************************************************/

#include <TAU.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include <Profile/TauEnv.h>
#include <Profile/TauMetrics.h>
#include <Profile/TauSampling.h>
#include <Profile/TauSnapshot.h>

extern "C" void Tau_stack_initialization();
extern "C" int Tau_compensate_initialization();
extern "C" int Tau_profiler_initialization();


/* -- signal catching to flush event buffers ----------------- */
#if defined (__cplusplus) || defined (__STDC__) || defined (_AIX) || (defined (__mips) && defined (_SYSTYPE_SVR4))
#define SIGNAL_TYPE	void
#define SIGNAL_ARG_TYPE	int
#else	/* Not ANSI C.  */
#define SIGNAL_TYPE	int
#define SIGNAL_ARG_TYPE
#endif	/* ANSI C */
# ifndef NSIG
#   define NSIG 32
# endif
static SIGNAL_TYPE (*sighdlr[NSIG])(SIGNAL_ARG_TYPE);

static void wrap_up(int sig) {
  fprintf (stderr, "TAU: signal %d on %d - flushing event buffer...\n", sig, RtsLayer::myNode());
  TAU_PROFILE_EXIT("signal");
  fprintf (stderr, "TAU: done.\n");
  exit (1);
}

static void TauInitialize_kill_handlers() {
# ifdef SIGINT
  sighdlr[SIGINT ] = signal (SIGINT , wrap_up);
# endif
# ifdef SIGQUIT
  sighdlr[SIGQUIT] = signal (SIGQUIT, wrap_up);
# endif
# ifdef SIGILL
  sighdlr[SIGILL ] = signal (SIGILL , wrap_up);
# endif
# ifdef SIGFPE
  sighdlr[SIGFPE ] = signal (SIGFPE , wrap_up);
# endif
# ifdef SIGBUS
  sighdlr[SIGBUS ] = signal (SIGBUS , wrap_up);
# endif
# ifdef SIGTERM
  sighdlr[SIGTERM] = signal (SIGTERM, wrap_up);
# endif
# ifdef SIGABRT
  sighdlr[SIGABRT] = signal (SIGABRT, wrap_up);
# endif
# ifdef SIGSEGV
  sighdlr[SIGSEGV] = signal (SIGSEGV, wrap_up);
# endif
}



static void tauSignalHandler(int sig) {
  fprintf (stderr, "Caught SIGUSR1, dumping TAU profile data\n");
  TAU_DB_DUMP_PREFIX("profile");
}

static void tauToggleInstrumentationHandler(int sig) {
  fprintf (stderr, "Caught SIGUSR2, toggling TAU instrumentation\n");
  if (RtsLayer::TheEnableInstrumentation()) {
    RtsLayer::TheEnableInstrumentation() = false;
  } else {
    RtsLayer::TheEnableInstrumentation() = true;
  }
}


static int tau_initialized = 0;

extern "C" int Tau_init_check_initialized() {
  return tau_initialized;
}


extern "C" int Tau_init_initializeTAU() {
  static int initialized = 0;

  if (initialized) {
    return 0;
  }

  Tau_global_incr_insideTAU();
  
  tau_initialized = 1;

  /* initialize the Profiler stack */
  Tau_stack_initialization();

  /* initialize environment variables */
  TauEnv_initialize();

#ifdef TAU_EPILOG
  /* no more initialization necessary if using epilog/scalasca */
  initialized = 1;
  return 0;
#endif

#ifdef TAU_VAMPIRTRACE
  /* no more initialization necessary if using vampirtrace */
  initialized = 1;
  return 0;
#endif
  
  /* we need the timestamp of the "start" */
  Tau_snapshot_initialization();



#ifndef TAU_DISABLE_SIGUSR
  /* register SIGUSR1 handler */
  if (signal(SIGUSR1, tauSignalHandler) == SIG_ERR) {
    perror("failed to register TAU profile dump signal handler");
  }

  if (signal(SIGUSR2, tauToggleInstrumentationHandler) == SIG_ERR) {
    perror("failed to register TAU instrumentation toggle signal handler");
  }
#endif


  Tau_profiler_initialization();

  /********************************************/
  /* other initialization code should go here */
  /********************************************/

  /* initialize the metrics we will be counting */
  TauMetrics_init();

  /* TAU must me marked as initialized BEFORE Tau_compensate_initialize is called
     Otherwise re-entry to this function will take place and bad things will happen */
  initialized = 1;

  /* initialize compensation */
  if (TauEnv_get_compensate()) {
    Tau_compensate_initialization();
  }

  /* initialize signal handlers to flush the trace buffer */
  if (TauEnv_get_tracing()) {
    TauInitialize_kill_handlers();
  }

  /* initialize sampling if requested */
  if (TauEnv_get_ebs_enabled()) {
    Tau_sampling_init(0);
  }

  Tau_global_decr_insideTAU();
  return 0;
}
