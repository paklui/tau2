/****************************************************************************
**			TAU Portable Profiling Package			   **
**			http://www.cs.uoregon.edu/research/tau	           **
*****************************************************************************
**    Copyright 2007  						   	   **
**    Department of Computer and Information Science, University of Oregon **
**    Advanced Computing Laboratory, Los Alamos National Laboratory        **
****************************************************************************/
/****************************************************************************
**	File 		: MetaData.cpp  				   **
**	Description 	: TAU Profiling Package				   **
**	Author		: Alan Morris					   **
**	Contact		: tau-bugs@cs.uoregon.edu               	   **
**	Documentation	: See http://www.cs.uoregon.edu/research/tau       **
**                                                                         **
**      Description     : This file contains all the Metadata, XML and     **
**                        Snapshot related routines                        **
**                                                                         **
****************************************************************************/

#ifndef TAU_DISABLE_METADATA
#include "tau_config.h"
#if defined(TAU_WINDOWS)
double TauWindowsUsecD(); // from RtsLayer.cpp
#else
#include <sys/utsname.h> // for host identification (uname)
#include <unistd.h>
#include <sys/time.h>
#endif /* TAU_WINDOWS */
#endif /* TAU_DISABLE_METADATA */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sstream>

#include "tauarch.h"
#include <Profile/Profiler.h>
#include <Profile/tau_types.h>
#include <Profile/TauMetrics.h>
#include <TauUtil.h>
#include <TauXML.h>
#include <TauMetaData.h>

// Moved from header file
using namespace std;


#ifdef TAU_BGL
#include <rts.h>
#include <bglpersonality.h>
#endif


/* Re-enabled since we believe this is now working (2009-11-02) */
/* 
   #ifdef TAU_IBM_XLC_BGP
   #undef TAU_BGP
   #endif
*/
/* NOTE: IBM BG/P XLC does not work with metadata when it is compiled with -qpic=large */


#ifdef TAU_BGP
/* header files for BlueGene/P */
#include <bgp_personality.h>
#include <bgp_personality_inlines.h>
#include <kernel_interface.h>
#endif // TAU_BGP

#ifdef TAU_BGQ
#include <firmware/include/personality.h>
#include <spi/include/kernel/process.h>
#include <spi/include/kernel/location.h>
#ifdef __GNUC__
#include <hwi/include/bqc/A2_inlines.h>
#endif
#include <hwi/include/common/uci.h>

static Personality_t tau_bgq_personality;
#define TAU_BGQ_TORUS_DIM 6
static int tau_torus_size[TAU_BGQ_TORUS_DIM];
static int tau_torus_coord[TAU_BGQ_TORUS_DIM];
static int tau_torus_wraparound[TAU_BGQ_TORUS_DIM];

int tau_bgq_init(void) {
  uint64_t network_options;

  Kernel_GetPersonality(&tau_bgq_personality, sizeof(Personality_t));

  tau_torus_size[0] = tau_bgq_personality.Network_Config.Anodes;
  tau_torus_size[1] = tau_bgq_personality.Network_Config.Bnodes;
  tau_torus_size[2] = tau_bgq_personality.Network_Config.Cnodes;
  tau_torus_size[3] = tau_bgq_personality.Network_Config.Dnodes;
  tau_torus_size[4] = tau_bgq_personality.Network_Config.Enodes;
  tau_torus_size[5] = 64;

  tau_torus_coord[0] = tau_bgq_personality.Network_Config.Anodes;
  tau_torus_coord[1] = tau_bgq_personality.Network_Config.Bnodes;
  tau_torus_coord[2] = tau_bgq_personality.Network_Config.Cnodes;
  tau_torus_coord[3] = tau_bgq_personality.Network_Config.Dnodes;
  tau_torus_coord[4] = tau_bgq_personality.Network_Config.Enodes;
  tau_torus_coord[5] = Kernel_ProcessorID();

  network_options = tau_bgq_personality.Network_Config.NetFlags;

  tau_torus_wraparound[0] = network_options & ND_ENABLE_TORUS_DIM_A;
  tau_torus_wraparound[1] = network_options & ND_ENABLE_TORUS_DIM_B;
  tau_torus_wraparound[2] = network_options & ND_ENABLE_TORUS_DIM_C;
  tau_torus_wraparound[3] = network_options & ND_ENABLE_TORUS_DIM_D;
  tau_torus_wraparound[4] = network_options & ND_ENABLE_TORUS_DIM_E;
  tau_torus_wraparound[5] = 0;

  return 1;
}

#endif /* TAU_BGQ */

#if (defined (TAU_CATAMOUNT) && defined (PTHREADS))
#define _BITS_PTHREADTYPES_H 1
#endif

#include <signal.h>
#include <stdarg.h>

// STL containers are not designed for this.
// They do not have virtual destructors, so overriding the destructor
// in this way is unsafe.  Is there some reason atexit() isn't enough?
class MetaDataRepo : public metadata_map_t {
public :
  virtual ~MetaDataRepo() {
    Tau_destructor_trigger();
  }
};


// These come from Tau_metadata_register calls
metadata_map_t & Tau_metadata_getMetaData_task(int tid) {
  static MetaDataRepo metadata[TAU_MAX_THREADS];
  return metadata[tid];
}

metadata_map_t & Tau_metadata_getMetaData(void) {
	return Tau_metadata_getMetaData_task(0);
}


extern "C" void Tau_metadata_task(char const * name, char const * value, int tid) {
#ifdef TAU_DISABLE_METADATA
  return;
#else
  // make copies
  char * myName = strdup(name);
  char * myValue = strdup(value);
  //TAU_VERBOSE("Metadata: %s = %s\n", name, value);
  RtsLayer::LockDB();
  Tau_metadata_getMetaData_task(tid)[myName] = myValue;
  RtsLayer::UnLockDB();
#endif
}
extern "C" void Tau_metadata(char *name, const char *value) {
	Tau_metadata_task(name, value, 0);
}


void Tau_metadata_register(char *name, int value) {
  char buf[256];
  sprintf (buf, "%d", value);
  Tau_metadata(name, buf);
}

void Tau_metadata_register(char *name, const char *value) {
  Tau_global_incr_insideTAU();
  Tau_metadata(name, value);
  Tau_global_decr_insideTAU();
}


int Tau_metadata_fillMetaData() 
{
#ifdef TAU_DISABLE_METADATA
  return 0;
#else

  static int filled = 0;
  if (filled) {
    return 0;
  }
  filled = 1;


#ifdef TAU_WINDOWS
  const char *timeFormat = "%I64d";
#else
  const char *timeFormat = "%lld";
#endif


  char tmpstr[4096];
  sprintf (tmpstr, timeFormat, TauMetrics_getInitialTimeStamp());
  Tau_metadata_register("Starting Timestamp", tmpstr);


  time_t theTime = time(NULL);
  struct tm *thisTime = gmtime(&theTime);
  strftime(tmpstr,4096,"%Y-%m-%dT%H:%M:%SZ", thisTime);
  Tau_metadata_register("UTC Time", tmpstr);


  thisTime = localtime(&theTime);
  char buf[4096];
  strftime (buf,4096,"%Y-%m-%dT%H:%M:%S", thisTime);

  char tzone[7];
  strftime (tzone, 7, "%z", thisTime);
  if (strlen(tzone) == 5) {
    tzone[6] = 0;
    tzone[5] = tzone[4];
    tzone[4] = tzone[3];
    tzone[3] = ':';
  }
  sprintf (tmpstr, "%s%s", buf, tzone);

  Tau_metadata_register("Local Time", tmpstr);

  // write out the timestamp (number of microseconds since epoch (unsigned long long)
  sprintf (tmpstr, timeFormat, TauMetrics_getTimeOfDay());
  Tau_metadata_register("Timestamp", tmpstr);


#ifndef TAU_WINDOWS
  // try to grab meta-data
  char hostname[4096];
  gethostname(hostname,4096);
  Tau_metadata_register("Hostname", hostname);

  struct utsname archinfo;

  uname (&archinfo);
  Tau_metadata_register("OS Name", archinfo.sysname);
  Tau_metadata_register("OS Version", archinfo.version);
  Tau_metadata_register("OS Release", archinfo.release);
  Tau_metadata_register("OS Machine", archinfo.machine);
  Tau_metadata_register("Node Name", archinfo.nodename);

  Tau_metadata_register("TAU Architecture", TAU_ARCH);
  Tau_metadata_register("TAU Config", TAU_CONFIG);
  Tau_metadata_register("TAU Makefile", TAU_MAKEFILE);
  Tau_metadata_register("TAU Version", TAU_VERSION);

  Tau_metadata_register("pid", (int)getpid());
#endif

#ifdef TAU_BGL
  char bglbuffer[4096];
  char location[BGLPERSONALITY_MAX_LOCATION];
  BGLPersonality personality;

  rts_get_personality(&personality, sizeof(personality));
  BGLPersonality_getLocationString(&personality, location);

  sprintf (bglbuffer, "(%d,%d,%d)", BGLPersonality_xCoord(&personality),
      BGLPersonality_yCoord(&personality),
      BGLPersonality_zCoord(&personality));
  Tau_metadata_register("BGL Coords", bglbuffer);

  Tau_metadata_register("BGL Processor ID", rts_get_processor_id());

  sprintf (bglbuffer, "(%d,%d,%d)", BGLPersonality_xSize(&personality),
      BGLPersonality_ySize(&personality),
      BGLPersonality_zSize(&personality));
  Tau_metadata_register("BGL Size", bglbuffer);


  if (BGLPersonality_virtualNodeMode(&personality)) {
    Tau_metadata_register("BGL Node Mode", "Virtual");
  } else {
    Tau_metadata_register("BGL Node Mode", "Coprocessor");
  }

  sprintf (bglbuffer, "(%d,%d,%d)", BGLPersonality_isTorusX(&personality),
      BGLPersonality_isTorusY(&personality),
      BGLPersonality_isTorusZ(&personality));
  Tau_metadata_register("BGL isTorus", bglbuffer);

  Tau_metadata_register("BGL DDRSize", BGLPersonality_DDRSize(&personality));
  Tau_metadata_register("BGL DDRModuleType", personality.DDRModuleType);
  Tau_metadata_register("BGL Location", location);

  Tau_metadata_register("BGL rankInPset", BGLPersonality_rankInPset(&personality));
  Tau_metadata_register("BGL numNodesInPset", BGLPersonality_numNodesInPset(&personality));
  Tau_metadata_register("BGL psetNum", BGLPersonality_psetNum(&personality));
  Tau_metadata_register("BGL numPsets", BGLPersonality_numPsets(&personality));

  sprintf (bglbuffer, "(%d,%d,%d)", BGLPersonality_xPsetSize(&personality),
      BGLPersonality_yPsetSize(&personality),
      BGLPersonality_zPsetSize(&personality));
  Tau_metadata_register("BGL PsetSize", bglbuffer);

  sprintf (bglbuffer, "(%d,%d,%d)", BGLPersonality_xPsetOrigin(&personality),
      BGLPersonality_yPsetOrigin(&personality),
      BGLPersonality_zPsetOrigin(&personality));
  Tau_metadata_register("BGL PsetOrigin", bglbuffer);

  sprintf (bglbuffer, "(%d,%d,%d)", BGLPersonality_xPsetCoord(&personality),
      BGLPersonality_yPsetCoord(&personality),
      BGLPersonality_zPsetCoord(&personality));
  Tau_metadata_register("BGL PsetCoord", bglbuffer);
#endif /* TAU_BGL */

#ifdef TAU_BGP
  char bgpbuffer[4096];
  char location[BGPPERSONALITY_MAX_LOCATION];
  _BGP_Personality_t personality;

  Kernel_GetPersonality(&personality, sizeof(_BGP_Personality_t));
  BGP_Personality_getLocationString(&personality, location);

  sprintf (bgpbuffer, "(%d,%d,%d)", BGP_Personality_xCoord(&personality),
      BGP_Personality_yCoord(&personality),
      BGP_Personality_zCoord(&personality));
  Tau_metadata_register("BGP Coords", bgpbuffer);

  Tau_metadata_register("BGP Processor ID", Kernel_PhysicalProcessorID());

  sprintf (bgpbuffer, "(%d,%d,%d)", BGP_Personality_xSize(&personality),
      BGP_Personality_ySize(&personality),
      BGP_Personality_zSize(&personality));
  Tau_metadata_register("BGP Size", bgpbuffer);


  if (Kernel_ProcessCount() > 1) {
    Tau_metadata_register("BGP Node Mode", "Virtual");
  } else {
    sprintf(bgpbuffer, "Coprocessor (%d)", Kernel_ProcessCount());
    Tau_metadata_register("BGP Node Mode", bgpbuffer);
  }

  sprintf (bgpbuffer, "(%d,%d,%d)", BGP_Personality_isTorusX(&personality),
      BGP_Personality_isTorusY(&personality),
      BGP_Personality_isTorusZ(&personality));
  Tau_metadata_register("BGP isTorus", bgpbuffer);

  Tau_metadata_register("BGP DDRSize (MB)", BGP_Personality_DDRSizeMB(&personality));
  /* CHECK: 
     Tau_metadata_register("BGP DDRModuleType", personality.DDRModuleType);
   */
  Tau_metadata_register("BGP Location", location);

  Tau_metadata_register("BGP rankInPset", BGP_Personality_rankInPset(&personality));
  /*
     Tau_metadata_register("BGP numNodesInPset", Kernel_ProcessCount());
   */
  Tau_metadata_register("BGP psetSize", BGP_Personality_psetSize(&personality));
  Tau_metadata_register("BGP psetNum", BGP_Personality_psetNum(&personality));
  Tau_metadata_register("BGP numPsets", BGP_Personality_numComputeNodes(&personality));

  /* CHECK: 
     sprintf (bgpbuffer, "(%d,%d,%d)", BGP_Personality_xPsetSize(&personality),
     BGP_Personality_yPsetSize(&personality),
     BGP_Personality_zPsetSize(&personality));
     Tau_metadata_register("BGP PsetSize", bgpbuffer);

     sprintf (bgpbuffer, "(%d,%d,%d)", BGP_Personality_xPsetOrigin(&personality),
     BGP_Personality_yPsetOrigin(&personality),
     BGP_Personality_zPsetOrigin(&personality));
     Tau_metadata_register("BGP PsetOrigin", bgpbuffer);

     sprintf (bgpbuffer, "(%d,%d,%d)", BGP_Personality_xPsetCoord(&personality),
     BGP_Personality_yPsetCoord(&personality),
     BGP_Personality_zPsetCoord(&personality));
     Tau_metadata_register("BGP PsetCoord", bgpbuffer);
   */

#endif /* TAU_BGP */

#ifdef TAU_BGQ
  /* NOTE: Please refer to Scalasca's elg_pform_bgq.c [www.scalasca.org] for 
     details on IBM BGQ Axis mapping. */
  static int bgq_init = tau_bgq_init(); 
  char bgqbuffer[4096];
  static char tau_axis_map[] = "EFABCD";  
  /* EF -> x, AB -> y, CD -> z */

#define TAU_BGQ_IDX(i) tau_axis_map[i] - 'A'

  int x = tau_torus_coord[TAU_BGQ_IDX(0)] * tau_torus_size[TAU_BGQ_IDX(1)] 	
    + tau_torus_coord[TAU_BGQ_IDX(1)]; 
  int y = tau_torus_coord[TAU_BGQ_IDX(2)] * tau_torus_size[TAU_BGQ_IDX(3)] 
    + tau_torus_coord[TAU_BGQ_IDX(3)]; 
  int z = tau_torus_coord[TAU_BGQ_IDX(4)] * tau_torus_size[TAU_BGQ_IDX(5)]
    + tau_torus_coord[TAU_BGQ_IDX(5)]; 

  sprintf(bgqbuffer, "(%d,%d,%d)", x,y,z);
  Tau_metadata_register("BGQ Coords", bgqbuffer);

  int size_x = tau_torus_size[TAU_BGQ_IDX(0)] * tau_torus_size[TAU_BGQ_IDX(1)];
  int size_y = tau_torus_size[TAU_BGQ_IDX(2)] * tau_torus_size[TAU_BGQ_IDX(3)];
  int size_z = tau_torus_size[TAU_BGQ_IDX(4)] * tau_torus_size[TAU_BGQ_IDX(5)];

  sprintf(bgqbuffer, "(%d,%d,%d,%d,%d,%d)", tau_torus_size[0], tau_torus_size[1], tau_torus_size[2],
      tau_torus_size[3], tau_torus_size[4], tau_torus_size[5]);
  Tau_metadata_register("BGQ Size", bgqbuffer);

  int wrap_x = tau_torus_wraparound[TAU_BGQ_IDX(0)] && tau_torus_wraparound[TAU_BGQ_IDX(1)]; 
  int wrap_y = tau_torus_wraparound[TAU_BGQ_IDX(2)] && tau_torus_wraparound[TAU_BGQ_IDX(3)]; 
  int wrap_z = tau_torus_wraparound[TAU_BGQ_IDX(4)] && tau_torus_wraparound[TAU_BGQ_IDX(5)]; 

  sprintf(bgqbuffer, "(%d,%d,%d)", wrap_x,wrap_y,wrap_z);
  Tau_metadata_register("BGQ Period", bgqbuffer);

  BG_UniversalComponentIdentifier uci = tau_bgq_personality.Kernel_Config.UCI;
  unsigned int row, col, mp, nb, cc;
  bg_decodeComputeCardOnNodeBoardUCI(uci, &row, &col, &mp, &nb, &cc);
  sprintf(bgqbuffer, "R%x%x-M%d-N%02x-J%02x <%d,%d,%d,%d,%d>", row, col, mp, nb, cc,
      tau_torus_coord[0], tau_torus_coord[1], tau_torus_coord[2],
      tau_torus_coord[3], tau_torus_coord[4]);
  Tau_metadata_register("BGQ Node Name", bgqbuffer);

  sprintf(bgqbuffer, "%ld", ((uci>>38)&0xFFFFF)); /* encode row,col,mp,nb,cc*/
  Tau_metadata_register("BGQ Node ID", bgqbuffer);

  sprintf(bgqbuffer, "%ld", Kernel_PhysicalProcessorID());
  Tau_metadata_register("BGQ Physical Processor ID", bgqbuffer);

  sprintf(bgqbuffer, "%d", tau_bgq_personality.Kernel_Config.FreqMHz);
  Tau_metadata_register("CPU MHz", bgqbuffer);

  sprintf(bgqbuffer, "%d", Kernel_GetJobID());
  Tau_metadata_register("BGQ Job ID", bgqbuffer);

  sprintf(bgqbuffer, "%d", Kernel_ProcessorID());
  Tau_metadata_register("BGQ Processor ID", bgqbuffer);

  sprintf(bgqbuffer, "%d", Kernel_PhysicalHWThreadID());
  Tau_metadata_register("BGQ Physical HW Thread ID", bgqbuffer);

  sprintf(bgqbuffer, "%d", Kernel_ProcessCount());
  Tau_metadata_register("BGQ Process Count", bgqbuffer);

  sprintf(bgqbuffer, "%d", Kernel_ProcessorCount());
  Tau_metadata_register("BGQ Processor Count", bgqbuffer);

  sprintf(bgqbuffer, "%d", Kernel_MyTcoord());
  Tau_metadata_register("BGQ tCoord", bgqbuffer);

  sprintf(bgqbuffer, "%d", Kernel_ProcessorCoreID());
  Tau_metadata_register("BGQ Processor Core ID", bgqbuffer);

  sprintf(bgqbuffer, "%d", Kernel_ProcessorThreadID());
  Tau_metadata_register("BGQ Processor Thread ID", bgqbuffer);

  sprintf(bgqbuffer, "%d", Kernel_BlockThreadId());
  Tau_metadata_register("BGQ Block Thread ID", bgqbuffer);

  // Returns the Rank associated with the current process
  sprintf(bgqbuffer, "%d", Kernel_GetRank());
  Tau_metadata_register("BGQ Rank", bgqbuffer);

  sprintf(bgqbuffer, "%d", tau_bgq_personality.DDR_Config.DDRSizeMB);
  Tau_metadata_register("BGQ DDR Size (MB)", bgqbuffer);


#endif /* TAU_BGQ */

#ifdef __linux__
  // doesn't work on ia64 for some reason
  //Tau_util_output (out, "\t<linux_tid>%d</linux_tid>\n", gettid());

  // try to grab CPU info
  FILE *f = fopen("/proc/cpuinfo", "r");
  if (f) {
    char line[4096];
    while (Tau_util_readFullLine(line, f)) {
      char const * value = strstr(line,":");
      if (!value) {
        break;
      } else {
        /* skip over colon */
        value += 2;
      }

      // Allocates a string
      value = Tau_util_removeRuns(value);

      if (strncmp(line, "vendor_id", 9) == 0) {
        Tau_metadata_register("CPU Vendor", value);
      }
      if (strncmp(line, "vendor", 6) == 0) {
        Tau_metadata_register("CPU Vendor", value);
      }
      if (strncmp(line, "cpu MHz", 7) == 0) {
        Tau_metadata_register("CPU MHz", value);
      }
      if (strncmp(line, "clock", 5) == 0) {
        Tau_metadata_register("CPU MHz", value);
      }
      if (strncmp(line, "model name", 10) == 0) {
        Tau_metadata_register("CPU Type", value);
      }
      if (strncmp(line, "family", 6) == 0) {
        Tau_metadata_register("CPU Type", value);
      }
      if (strncmp(line, "cpu\t", 4) == 0) {
        Tau_metadata_register("CPU Type", value);
      }
      if (strncmp(line, "cache size", 10) == 0) {
        Tau_metadata_register("Cache Size", value);
      }
      if (strncmp(line, "cpu cores", 9) == 0) {
        Tau_metadata_register("CPU Cores", value);
      }

      // Deallocates the string
      free((void*)value);
    }
    fclose(f);
  }

  f = fopen("/proc/meminfo", "r");
  if (f) {
    char line[4096];
    while (Tau_util_readFullLine(line, f)) {
      char const * value = strstr(line,":");

      if (!value) {
        break;
      } else {
        value += 2;
      }

      // Allocates a string
      value = Tau_util_removeRuns(value);

      if (strncmp(line, "MemTotal", 8) == 0) {
        Tau_metadata_register("Memory Size", value);
      }

      free((void*)value);
    }
    fclose(f);
  }

  char buffer[4096];
  bzero(buffer, 4096);
  int rc = readlink("/proc/self/exe", buffer, 4096);
  if (rc != -1) {
    Tau_metadata_register("Executable", buffer);
  }
  bzero(buffer, 4096);
  rc = readlink("/proc/self/cwd", buffer, 4096);
  if (rc != -1) {
    Tau_metadata_register("CWD", buffer);
  }


  f = fopen("/proc/self/cmdline", "r");
  if (f) {
    char line[4096];

    /* *CWL* - STL cannot be used in PGI init sections???
       std::ostringstream os;

       while (Tau_util_readFullLine(line, f)) {
       if (os.str().length() != 0) {
       os << " ";
       }
       os << line;
       }
       Tau_metadata_register("Command Line", os.str().c_str());
     */
    string os;
    // *CWL* - The following loop performs newline to space conversions
    while (Tau_util_readFullLine(line, f)) {
      if (os.length() != 0) {
        os.append(" ");
      }
      os.append(string(line));
    }    
    Tau_metadata_register("Command Line", os.c_str());
    fclose(f);
  }
#endif /* __linux__ */

  char *user = getenv("USER");
  if (user != NULL) {
    Tau_metadata_register("username", user);
  }

  return 0;
#endif

}


static int writeMetaData(Tau_util_outputDevice *out, bool newline, int counter, int tid) {
  const char *endl = "";
  if (newline) {
    endl = "\n";
  }
  Tau_util_output (out, "<metadata>%s", endl);

  if (counter != -1) {
    Tau_XML_writeAttribute(out, "Metric Name", RtsLayer::getCounterName(counter), newline);
  }

  // Write data from the Tau_metadata_register environment variable
  // char *tauMetaDataEnvVar = getenv("Tau_metadata_register");
  // if (tauMetaDataEnvVar != NULL) {
  //   if (strncmp(tauMetaDataEnvVar, "<attribute>", strlen("<attribute>")) != 0) {
  //     fprintf (stderr, "Error in formating TAU_METADATA environment variable\n");
  //   } else {
  //     Tau_util_output (out, tauMetaDataEnvVar);
  //   }
  // }

  // write out the user-specified (some from TAU) attributes
  metadata_map_t const & metadata = Tau_metadata_getMetaData_task(tid);
  for (metadata_map_t::const_iterator it=metadata.begin(); it != metadata.end(); it++) {
    const char *name = it->first;
    const char *value = it->second;
    Tau_XML_writeAttribute(out, name, value, newline);
  }

  Tau_util_output (out, "</metadata>%s", endl);
  return 0;
}





extern "C" void Tau_context_metadata(char *name, char *value) {
#ifdef TAU_DISABLE_METADATA
  return;
#else
  // get the current calling context
  Profiler *current = TauInternal_CurrentProfiler(RtsLayer::getTid());
  FunctionInfo *fi = current->ThisFunction;
  const char *fname = fi->GetName();

  char *myName = (char*) malloc (strlen(name) + strlen(fname) + 10);
  sprintf(myName, "%s => %s", fname, name);
  char *myValue = strdup(value);
  RtsLayer::LockDB();
  Tau_metadata_getMetaData()[myName] = myValue;
  RtsLayer::UnLockDB();
#endif
}

extern "C" void Tau_phase_metadata(char *name, char *value) {
#ifdef TAU_DISABLE_METADATA
  return;
#else
#ifdef TAU_PROFILEPHASE
  // get the current calling context
  Profiler *current = TauInternal_CurrentProfiler(RtsLayer::getTid());
  std::string myString = "";
  while (current != NULL) {
    if (current->GetPhase()) {
      FunctionInfo *fi = current->ThisFunction;
      const char *fname = fi->GetName();
      myString = std::string(fname) + " => " + myString;
    }    
    current = current->ParentProfiler;
  }

  myString = myString + name;
  char *myName = strdup(myString.c_str());
  char *myValue = strdup(value);
 
  RtsLayer::LockDB();
  Tau_metadata_getMetaData()[myName] = myValue;
  RtsLayer::UnLockDB();
#else
  Tau_context_metadata(name, value);
#endif
#endif
}


int Tau_metadata_writeMetaData(Tau_util_outputDevice *out, int tid) {

#ifdef TAU_DISABLE_METADATA
  return 0;
#endif

  //Tau_metadata_fillMetaData();
  return writeMetaData(out, true, -1, tid);
}

int Tau_metadata_writeMetaData(Tau_util_outputDevice *out) {
  return writeMetaData(out, true, -1, 0);
}

int Tau_metadata_writeMetaData(Tau_util_outputDevice *out, int counter, int tid) {
#ifdef TAU_DISABLE_METADATA
  return 0;
#endif

  //Tau_metadata_fillMetaData();
  int retval;
  retval = writeMetaData(out, false, counter, tid);
  return retval;
}

/* helper function to write to already established file pointer */
int Tau_metadata_writeMetaData(FILE *fp, int counter, int tid) {
  Tau_util_outputDevice out;
  out.fp = fp;
  out.type = TAU_UTIL_OUTPUT_FILE;
  return Tau_metadata_writeMetaData(&out, counter, tid);
}


Tau_util_outputDevice *Tau_metadata_generateMergeBuffer() {
  Tau_util_outputDevice *out = Tau_util_createBufferOutputDevice();

  Tau_util_output(out,"%d%c", Tau_metadata_getMetaData().size(), '\0');

  metadata_map_t const & metadata = Tau_metadata_getMetaData();
  for (metadata_map_t::const_iterator it=metadata.begin(); it != metadata.end(); it++) {
    const char *name = it->first;
    const char *value = it->second;
    Tau_util_output(out,"%s%c", name, '\0');
    Tau_util_output(out,"%s%c", value, '\0');
  }
  return out;
}


void Tau_metadata_removeDuplicates(char *buffer, int buflen) {
  // read the number of items and allocate arrays
  int numItems;
  sscanf(buffer,"%d", &numItems);
  buffer = strchr(buffer, '\0')+1;

  char **attributes = (char **) malloc(sizeof(char*) * numItems);
  char **values = (char **) malloc(sizeof(char*) * numItems);

  // assign char pointers to the values inside the buffer
  for (int i=0; i<numItems; i++) {
    const char *attribute = buffer;
    buffer = strchr(buffer, '\0')+1;
    const char *value = buffer;
    buffer = strchr(buffer, '\0')+1;

    metadata_map_t const & metadata = Tau_metadata_getMetaData();
    metadata_map_t::const_iterator it = metadata.find(attribute);
    if (it != metadata.end()) {
      const char *my_value = it->second;
      if (0 == strcmp(value, my_value)) {
        Tau_metadata_getMetaData().erase(attribute);
      }
    }
  }
}


