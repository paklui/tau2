/****************************************************************************
**			TAU Portable Profiling Package                     **
**			http://www.cs.uoregon.edu/research/tau             **
*****************************************************************************
**    Copyright 2009-2017  						   	   **
**    Department of Computer and Information Science, University of Oregon **
**    Advanced Computing Laboratory, Los Alamos National Laboratory        **
**    Forschungszentrum Juelich                                            **
****************************************************************************/
/****************************************************************************
**	File 		: TracerOTF2.cpp 			        	   **
**	Description 	: TAU Tracing for native OTF2 generation			   **
**  Author      : Nicholas Chaimov
**	Contact		: tau-bugs@cs.uoregon.edu               	   **
**	Documentation	: See http://www.cs.uoregon.edu/research/tau       **
**                                                                         **
**      Description     : TAU Tracing                                      **
**                                                                         **
****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#include <tau_internal.h>
#include <Profile/Profiler.h>
#include <Profile/TauEnv.h>
#include <Profile/TauTrace.h>
#include <Profile/TauTraceOTF2.h>
#include <Profile/TauMetrics.h>
#include <Profile/TauCollectives.h>


#include <iostream>
#include <sstream>
#include <map>
#include <set>

#include <otf2/otf2.h>
#ifdef PTHREADS
#include <otf2/OTF2_Pthread_Locks.h>
#endif
#ifdef TAU_OPENMP
#include <otf2/OTF2_OpenMP_Locks.h>
#endif

#ifdef TAU_INCLUDE_MPI_H_HEADER
#ifdef TAU_MPI
#include <mpi.h>
#endif 
#endif /* TAU_INCLUDE_MPI_H_HEADER */


#define OTF2_EC(call) { \
    OTF2_ErrorCode ec = call; \
    if (ec != OTF2_SUCCESS) { \
        printf("TAU: OTF2 Error (%s:%d): %s, %s\n", __FILE__, __LINE__, OTF2_Error_GetName(ec), OTF2_Error_GetDescription (ec)); \
        abort(); \
    } \
}

using namespace std;
using namespace tau;

static bool otf2_initialized = false;
static bool otf2_finished = false;
static OTF2_Archive * otf2_archive = NULL;
static x_uint64 start_time = 0;
static x_uint64 end_time = 0;

static int * num_locations = NULL;
static uint64_t * num_events_written = NULL;
static int * num_regions = NULL;
static int * region_db_sizes = NULL; 
static char * region_names = NULL;

typedef map<string,uint64_t> region_map_t;
static region_map_t global_region_map;

extern "C" x_uint64 TauTraceGetTimeStamp(int tid);
extern "C" int tau_totalnodes(int set_or_get, int value);

// Collective Callbacks -- GetSize and GetRank are mandatory
// others are only needed when using SION substrate

static OTF2_CallbackCode tau_collectives_get_size(void*                   userData,
                                                  OTF2_CollectiveContext* commContext,
                                                  uint32_t*               size )
{
  *size = TauCollectives_get_size((TauCollectives_Group*) commContext);
  return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode tau_collectives_get_rank(void*                   userData,
                                                  OTF2_CollectiveContext* commContext,
                                                  uint32_t*               rank )
{
  *rank = RtsLayer::myNode();
  return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode
tau_collectives_barrier( void*                   userData,
                              OTF2_CollectiveContext* commContext )
{
    TauCollectives_Barrier((TauCollectives_Group*) commContext );

    return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode
tau_collectives_bcast( void*                   userData,
                            OTF2_CollectiveContext* commContext,
                            void*                   data,
                            uint32_t                numberElements,
                            OTF2_Type               type,
                            uint32_t                root )
{
    TauCollectives_Bcast( ( TauCollectives_Group* )commContext,
                           data,
                           numberElements,
                           TauCollectives_get_type( type ),
                           root );

    return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode
tau_collectives_gather( void*                   userData,
                             OTF2_CollectiveContext* commContext,
                             const void*             inData,
                             void*                   outData,
                             uint32_t                numberElements,
                             OTF2_Type               type,
                             uint32_t                root )
{
    TauCollectives_Gather( ( TauCollectives_Group* )commContext,
                            inData,
                            outData,
                            numberElements,
                            TauCollectives_get_type( type ),
                            root );

    return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode
tau_collectives_gatherv( void*                   userData,
                              OTF2_CollectiveContext* commContext,
                              const void*             inData,
                              uint32_t                inElements,
                              void*                   outData,
                              const uint32_t*         outElements,
                              OTF2_Type               type,
                                         uint32_t                root )
{
    TauCollectives_Gatherv( ( TauCollectives_Group* )commContext,
                             inData,
                             inElements,
                             outData,
                             ( const int* )outElements,
                             TauCollectives_get_type( type ),
                             root );

    return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode
tau_collectives_scatter( void*                   userData,
                              OTF2_CollectiveContext* commContext,
                              const void*             inData,
                              void*                   outData,
                              uint32_t                numberElements,
                              OTF2_Type               type,
                              uint32_t                root )
{
    TauCollectives_Scatter( ( TauCollectives_Group* )commContext,
                             inData,
                             outData,
                             numberElements,
                             TauCollectives_get_type( type ),
                             root );

    return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode
tau_collectives_scatterv( void*                   userData,
                               OTF2_CollectiveContext* commContext,
                               const void*             inData,
                               const uint32_t*         inElements,
                               void*                   outData,
                               uint32_t                outElements,
                               OTF2_Type               type,
                               uint32_t                root )
{
    TauCollectives_Scatterv( ( TauCollectives_Group* )commContext,
                              inData,
                              ( const int* )inElements,
                              outData,
                              outElements,
                              TauCollectives_get_type( type ),
                              root );

    return OTF2_CALLBACK_SUCCESS;
}

static const OTF2_CollectiveCallbacks tau_otf2_collectives =
{
    .otf2_release           = NULL,
    .otf2_get_size          = tau_collectives_get_size,
    .otf2_get_rank          = tau_collectives_get_rank,
    .otf2_create_local_comm = NULL,
    .otf2_free_local_comm   = NULL,
    .otf2_barrier           = tau_collectives_barrier,
    .otf2_bcast             = tau_collectives_bcast,
    .otf2_gather            = tau_collectives_gather,
    .otf2_gatherv           = tau_collectives_gatherv,
    .otf2_scatter           = tau_collectives_scatter,
    .otf2_scatterv          = tau_collectives_scatterv
};



// Flush Callbacks -- both mandatory

static OTF2_FlushType tau_OTF2PreFlush( void* userData, OTF2_FileType fileType,
        OTF2_LocationRef location, void* callerData, bool final ) {
    return OTF2_FLUSH;
}

static OTF2_TimeStamp tau_OTF2PostFlush(void* userData, OTF2_FileType fileType,
        OTF2_LocationRef location ) { 
    return TauTraceGetTimeStamp(0);
}

static OTF2_FlushCallbacks * get_tau_flush_callbacks() {
   static OTF2_FlushCallbacks cb;
   cb.otf2_pre_flush = tau_OTF2PreFlush;
   cb.otf2_post_flush = tau_OTF2PostFlush;
   return &cb;
}


// Helper functions

static inline OTF2_LocationRef my_location_offset() {
    const int64_t myNode = RtsLayer::myNode();
    return myNode == -1 ? 0 : (myNode * TAU_MAX_THREADS);
}

static inline OTF2_LocationRef my_location() {
    const int64_t myNode = RtsLayer::myNode();
    const int64_t myThread = RtsLayer::myThread();
    return myNode == -1 ? myThread : (myNode * TAU_MAX_THREADS) + myThread;
}

static inline uint32_t my_node() {
    const int myRtsNode = RtsLayer::myNode();
    const uint32_t my_node = myRtsNode == -1 ? 0 : myRtsNode;
    return my_node;
}

// Tau Tracing API calls for OTF2

/* Flush the trace buffer */
void TauTraceOTF2FlushBuffer(int tid)
{
  // Protect TAU from itself
  TauInternalFunctionGuard protects_this_function;
}

/* Initialize tracing. */
int TauTraceOTF2Init(int tid) {
  return TauTraceOTF2InitTS(tid, TauTraceGetTimeStamp(tid));
}

int TauTraceOTF2InitTS(int tid, x_uint64 ts)
{
  TauInternalFunctionGuard protects_this_function;     
  if(otf2_initialized || otf2_finished) {
      return 0;
  }
  start_time = ts;
  otf2_archive = OTF2_Archive_Open(TauEnv_get_tracedir() /* path */,
                             "trace" /* filename */,
                             OTF2_FILEMODE_WRITE,
                             OTF2_CHUNK_SIZE_EVENTS_DEFAULT,
                             OTF2_CHUNK_SIZE_DEFINITIONS_DEFAULT,
                             OTF2_SUBSTRATE_POSIX,
                             OTF2_COMPRESSION_NONE);
  TAU_ASSERT(otf2_archive != NULL, "Unable to create new OTF2 archive");

  OTF2_EC(OTF2_Archive_SetFlushCallbacks(otf2_archive, get_tau_flush_callbacks(), NULL));
  TauCollectives_Init();
  OTF2_EC(OTF2_Archive_SetCollectiveCallbacks(otf2_archive, &tau_otf2_collectives, NULL, ( OTF2_CollectiveContext* )TauCollectives_Get_World(), NULL));
  OTF2_EC(OTF2_Archive_SetCreator(otf2_archive, "TAU"));
#if defined(TAU_OPENMP)
  OTF2_EC(OTF2_OpenMP_Archive_SetLockingCallbacks(otf2_archive));
#elif defined(PTHREADS)
  OTF2_EC(OTF2_Pthread_Archive_SetLockingCallbacks(otf2_archive, NULL));
#endif
  // If going to use a threading model other than OpenMP or Pthreads,
  // a set of custom locking callbacks will need to be defined.

  OTF2_EC(OTF2_Archive_OpenEvtFiles(otf2_archive));
  OTF2_EC(OTF2_Archive_OpenDefFiles(otf2_archive));

  OTF2_EvtWriter* evt_writer = OTF2_Archive_GetEvtWriter(otf2_archive, my_location());
  TAU_ASSERT(evt_writer != NULL, "Failed to open new event writer");

  otf2_initialized = true;
  return 0; 
}

/* This routine is typically invoked when multiple SET_NODE calls are 
   encountered for a multi-threaded program */ 
void TauTraceOTF2Reinitialize(int oldid, int newid, int tid) {
  // TODO find tid's location and call OTF2_EvtWriter_SetLocationID
  return ;
}

/* Reset the trace */
void TauTraceOTF2UnInitialize(int tid) {
  /* to set the trace as uninitialized and clear the current buffers (for forked
     child process, trying to clear its parent records) */
}


/* Write event to buffer */
void TauTraceOTF2EventSimple(long int ev, x_int64 par, int tid, int kind) {
  TauTraceOTF2Event(ev, par, tid, 0, 0, kind);
}

/* Write event to buffer */
void TauTraceOTF2EventWithNodeId(long int ev, x_int64 par, int tid, x_uint64 ts, int use_ts, int node_id, int kind)
{
  TauInternalFunctionGuard protects_this_function;
  if(otf2_finished) {
    return;
  }
  if(!otf2_initialized) {
#ifdef TAU_MPI
    // If we're using MPI, we can't initialize tracing until MPI_Init gets called,
    // which will in turn init tracing for us, so we can't do it here.
    // This is because when we call OTF2_Archive_Open and set the collective callbacks,
    // we must know our rank, because at that time rank 0 alone must create the trace
    // directory.
    return;
#else
    if(use_ts) {
        TauTraceOTF2InitTS(tid, ts); 
    } else {
        TauTraceOTF2Init(tid);
    }
#endif
  }
  if(kind == TAU_TRACE_EVENT_KIND_FUNC) {
    int loc = my_location();
    OTF2_EvtWriter* evt_writer = OTF2_Archive_GetEvtWriter(otf2_archive, loc);
    TAU_ASSERT(evt_writer != NULL, "Failed to get event writer");
    if(par == 1) { // Enter
      OTF2_EC(OTF2_EvtWriter_Enter(evt_writer, NULL, use_ts ? ts : TauTraceGetTimeStamp(tid), ev));
    } else if(par == -1) { // Exit
      OTF2_EC(OTF2_EvtWriter_Leave(evt_writer, NULL, use_ts ? ts : TauTraceGetTimeStamp(tid), ev));
    }
  }
}


extern "C" void TauTraceOTF2Msg(int send_or_recv, int type, int other_id, int length, x_uint64 ts, int use_ts, int node_id) {

}

/* Write event to buffer */
void TauTraceOTF2Event(long int ev, x_int64 par, int tid, x_uint64 ts, int use_ts, int kind) {
  TauTraceOTF2EventWithNodeId(ev, par, tid, ts, use_ts, my_node(), kind);
}

static void TauTraceOTF2WriteGlobalDefinitions() {
    TauInternalFunctionGuard protects_this_function;
    OTF2_GlobalDefWriter * global_def_writer = OTF2_Archive_GetGlobalDefWriter(otf2_archive);
    TAU_ASSERT(global_def_writer != NULL, "Failed to get global def writer");

    OTF2_GlobalDefWriter_WriteClockProperties(global_def_writer, 1000000, start_time, end_time - start_time);

    // Write a Location for each thread within each Node (which has a LocationGroup and SystemTreeNode)
        
    int nextString = 1;
    const int emptyString = 0;
    OTF2_EC(OTF2_GlobalDefWriter_WriteString(global_def_writer, 0, "" ));

    const int nodes = tau_totalnodes(0, 0);
    int thread_num = 0;
    for(int node = 0; node < nodes; ++node) {
        // System Tree Node
        char namebuf[256];
        // TODO hostname
        snprintf(namebuf, 256, "node %d", node);                                  
        int nodeName = nextString++;
        OTF2_EC(OTF2_GlobalDefWriter_WriteString(global_def_writer, nodeName, namebuf));
        OTF2_EC(OTF2_GlobalDefWriter_WriteSystemTreeNode(global_def_writer, node, nodeName, emptyString, OTF2_UNDEFINED_SYSTEM_TREE_NODE));        

        // Location Group
        snprintf(namebuf, 256, "group %d", node);
        int groupName = nextString++;
        OTF2_EC(OTF2_GlobalDefWriter_WriteString(global_def_writer, groupName, namebuf));
        OTF2_EC(OTF2_GlobalDefWriter_WriteLocationGroup(global_def_writer, node, groupName, OTF2_LOCATION_GROUP_TYPE_PROCESS, node));

        const int start_loc = node * TAU_MAX_THREADS;
        const int end_loc = start_loc + num_locations[node];
        for(int loc = start_loc; loc < end_loc; ++loc) {
            snprintf(namebuf, 256, "thread %d", thread_num++);
            int locName = nextString++;
            OTF2_EC(OTF2_GlobalDefWriter_WriteString(global_def_writer, locName, namebuf));
            OTF2_EC(OTF2_GlobalDefWriter_WriteLocation(global_def_writer, loc, locName, OTF2_LOCATION_TYPE_CPU_THREAD, num_events_written[node], node));
        }

    }

    // Write all the functions out as Regions
    for (region_map_t::const_iterator it = global_region_map.begin(); it != global_region_map.end(); it++) {
        int thisFuncName = nextString++;
        OTF2_EC(OTF2_GlobalDefWriter_WriteString(global_def_writer, thisFuncName, it->first.c_str()));
        OTF2_EC(OTF2_GlobalDefWriter_WriteRegion(global_def_writer, it->second, thisFuncName, thisFuncName, emptyString, OTF2_REGION_ROLE_FUNCTION, OTF2_PARADIGM_USER, OTF2_REGION_FLAG_NONE, 0, 0, 0));
    }

    OTF2_EC(OTF2_Archive_CloseGlobalDefWriter(otf2_archive, global_def_writer));


}

static void TauTraceOTF2WriteLocalDefinitions() {
    TauInternalFunctionGuard protects_this_function;
    OTF2_IdMap * region_map = OTF2_IdMap_Create(OTF2_ID_MAP_SPARSE, TheFunctionDB().size());
    for (vector<FunctionInfo*>::iterator it = TheFunctionDB().begin(); it != TheFunctionDB().end(); it++) {
        FunctionInfo * fi = *it;
        if(tau_totalnodes(0,0) < 2) {
            OTF2_EC(OTF2_IdMap_AddIdPair(region_map, fi->GetFunctionId(), fi->GetFunctionId())); // identity map
        } else {
            const uint64_t local_id  = fi->GetFunctionId();
            const uint64_t global_id = global_region_map[string(fi->GetName())];
            OTF2_EC(OTF2_IdMap_AddIdPair(region_map, local_id, global_id));
        }
    }
    const int start_loc = my_location();
    const int end_loc = start_loc + RtsLayer::getTotalThreads();
    for(int loc = start_loc; loc < end_loc; ++loc) {
        OTF2_DefWriter* def_writer = OTF2_Archive_GetDefWriter(otf2_archive, loc);
        OTF2_EC(OTF2_DefWriter_WriteMappingTable(def_writer, OTF2_MAPPING_REGION, region_map));
        OTF2_EC(OTF2_Archive_CloseDefWriter(otf2_archive, def_writer));
    }
    OTF2_IdMap_Free(region_map);

}

static void TauTraceOTF2ExchangeLocations() {
    TauInternalFunctionGuard protects_this_function;
    if(my_node() == 0) {
        num_locations = new int[tau_totalnodes(0,0)];
    }
    const uint32_t my_num_threads = RtsLayer::getTotalThreads();
    if(tau_totalnodes(0,0) == 1) {
        num_locations[0] = my_num_threads;
    } else {
        TauCollectives_Gather(TauCollectives_Get_World(), &my_num_threads, num_locations, 1, TAUCOLLECTIVES_UINT32_T, 0);
    }
}

static void TauTraceOTF2ExchangeEventsWritten() {
    TauInternalFunctionGuard protects_this_function;
    const int nodes = tau_totalnodes(0, 0);
    int total_locs = 0;
    if(my_node() == 0) {
        for(int i = 0; i < nodes; ++i) {
            total_locs += num_locations[i];    
        }
        num_events_written = new uint64_t[total_locs];
    }
    const uint32_t my_num_threads = RtsLayer::getTotalThreads();
    uint64_t my_num_events[my_num_threads];
    const int offset = my_location_offset();
    for(OTF2_LocationRef i = 0; i < my_num_threads; ++i) {
        OTF2_EvtWriter * evt_writer = OTF2_Archive_GetEvtWriter(otf2_archive, offset + i);
        TAU_ASSERT(evt_writer != NULL, "Failed to get event writer");
        OTF2_EC(OTF2_EvtWriter_GetNumberOfEvents(evt_writer, my_num_events + i));
    }

    if(nodes == 1) {
        memcpy(num_events_written, my_num_events, my_num_threads);
    } else {
        TauCollectives_Gatherv(TauCollectives_Get_World(), &my_num_events, my_num_threads, num_events_written, num_locations, TAUCOLLECTIVES_UINT64_T, 0);
    }

}

static void TauTraceOTF2ExchangeRegions() {
    TauInternalFunctionGuard protects_this_function;
    // Collect local function IDs and names
    const int nodes = tau_totalnodes(0, 0);
    vector<uint64_t> function_ids;
    function_ids.reserve(TheFunctionDB().size());
    stringstream ss;
    for (vector<FunctionInfo*>::iterator it = TheFunctionDB().begin(); it != TheFunctionDB().end(); it++) {
        FunctionInfo *fi = *it;
        function_ids.push_back(fi->GetFunctionId());
        ss << fi->GetName();
        ss.put('\0');
    }
    string function_names_str = ss.str();
    const char * function_names = function_names_str.c_str();

    // Gather the sizes on master
    int my_num_regions = function_ids.size();
    if(my_node() == 0) {
        num_regions = new int[nodes];
    }
    TauCollectives_Gather(TauCollectives_Get_World(), &my_num_regions, num_regions, 1, TAUCOLLECTIVES_INT, 0);
    
    int my_region_db_size = function_names_str.size();
    if(my_node() == 0) {
        region_db_sizes = new int[nodes];
    }
    TauCollectives_Gather(TauCollectives_Get_World(), &my_region_db_size, region_db_sizes, 1, TAUCOLLECTIVES_INT, 0);

    // Exchange names
    int total_name_chars = 0;
    if(my_node() == 0) {
        for(int i = 0; i < nodes; ++i) {
            total_name_chars += region_db_sizes[i];
        }                
        region_names = new char[total_name_chars];
    
    }
    TauCollectives_Gatherv(TauCollectives_Get_World(), function_names, my_region_db_size, region_names, region_db_sizes, TAUCOLLECTIVES_CHAR, 0);

    // Create and distribute a map of all region names to global id
    char * global_regions = NULL;
    int global_regions_size = 0;
    if(my_node() == 0) {
        int name_offset = 0;
        set<string> unique_names;
        for(int node = 0; node < nodes; ++node) {
            const int node_num_regions = num_regions[node];
            for(int region = 0; region < node_num_regions; ++region) {
                string name = string(region_names+name_offset);    
                name_offset += name.length() + 1;
                unique_names.insert(name);
            }
        }

        int next_id = 0;
        for(set<string>::const_iterator it = unique_names.begin(); it != unique_names.end(); it++) {
            global_region_map[*it] = next_id++;
        }
        stringstream ss;
        for(region_map_t::const_iterator it = global_region_map.begin(); it != global_region_map.end(); it++) {
            ss << it->first;
            ss.put('\0');
        }
        string global_regions_str = ss.str();
        global_regions_size = global_regions_str.length();
        global_regions = (char *) malloc(global_regions_size * sizeof(char));
        global_regions = (char *) memcpy(global_regions, global_regions_str.c_str(), global_regions_size);    
    }

    TauCollectives_Bcast(TauCollectives_Get_World(), &global_regions_size, 1, TAUCOLLECTIVES_INT, 0);
    if(my_node() != 0) {
        global_regions = (char *)malloc(global_regions_size * sizeof(char));
    }

    TauCollectives_Bcast(TauCollectives_Get_World(), global_regions, global_regions_size, TAUCOLLECTIVES_CHAR, 0);

    if(my_node() != 0) {
        int region_offset = 0;
        int next_id = 0;
        while(region_offset < global_regions_size) {
            string name = string(global_regions+region_offset);
            region_offset += name.length() + 1;
            global_region_map[name] = next_id++;
        }
    }

    free(global_regions);

}

void TauTraceOTF2ShutdownComms(int tid) {
    TauInternalFunctionGuard protects_this_function;
    if(!otf2_initialized || otf2_finished) {
        return;
    }


    const int nodes = tau_totalnodes(0, 0);

    TauCollectives_Barrier(TauCollectives_Get_World());
    // Now everyone is at the beginning of MPI_Finalize()
    TauTraceOTF2ExchangeLocations();
    TauTraceOTF2ExchangeEventsWritten();
    TauTraceOTF2ExchangeRegions();

    TauCollectives_Finalize();

    TauTraceOTF2Close(tid);
}

/* Close the trace */
void TauTraceOTF2Close(int tid) {
    TauInternalFunctionGuard protects_this_function;
    if(tid != 0 || otf2_finished || !otf2_initialized) {
        return;
    }

    otf2_finished = true;
    otf2_initialized = false;
    end_time = TauTraceGetTimeStamp(0);

    // Write definitions file
    if(my_node() < 1) {
        TauTraceOTF2WriteGlobalDefinitions();
    }
    TauTraceOTF2WriteLocalDefinitions();
    
    
    OTF2_EC(OTF2_Archive_CloseEvtFiles(otf2_archive));
    OTF2_EC(OTF2_Archive_Close(otf2_archive));

    delete[] num_locations;
    delete[] num_events_written;
    delete[] num_regions;
    delete[] region_db_sizes;
    delete[] region_names;

}


