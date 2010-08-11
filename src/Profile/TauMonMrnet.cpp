// define these protocols only if monitoring is enabled. Otherwise,
//   TauMon.h will define default empty functions and we do not want
//   them interfering with one another.
#ifdef TAU_MONITORING

#include "Profile/TauMon.h"
#include "Profile/TauMonMrnet.h"

// for now, TAU_MONITORING will need to rely on TAU_EXP_UNIFY. This will
//   not be the case once event unification is implemented on every
//   available monitoring transport.
#ifdef TAU_EXP_UNIFY

#include <TAU.h>
#include <tau_types.h>
#include <TauUtil.h>
#include <TauMetrics.h>
#include <TauUnify.h>

#include <stdint.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <mpi.h>

#ifdef MRNET_LIGHTWEIGHT
extern "C" {
#endif /* MRNET_LIGHTWEIGHT */

#ifdef MRNET_LIGHTWEIGHT
#include "mrnet_lightweight/MRNet.h"
#else /* MRNET_LIGHTWEIGHT */
#include "mrnet/MRNet.h"
#endif /* MRNET_LIGHTWEIGHT */

#ifndef MRNET_LIGHTWEIGHT
using namespace MRN;
using namespace std;
#endif

extern "C" int getTomFlattenedIdx(int numCounters, int numTypes,
				  int evtIdx, int ctrIdx, int typeIdx) {
  return evtIdx*numCounters*numTypes + ctrIdx*numTypes + typeIdx;
}

// Back-end rank
int rank;

// Using a global for now. Could make it object-based like Aroon's old
//   codes later.
#ifdef MRNET_LIGHTWEIGHT
Network_t *net;
Stream_t *ctrl_stream = (Stream_t *)malloc(sizeof(Stream_t));
#else /* MRNET_LIGHTWEIGHT */
Network *net;
Stream *ctrl_stream;
#endif /* MRNET_LIGHTWEIGHT */

// Determine whether to extend protocol to receive results from the FE.
bool broadcastResults;

const char *profiledir;

// Unification structures
FunctionEventLister *mrnetFuncEventLister;
Tau_unify_object_t *mrnetFuncUnifier;
AtomicEventLister *mrnetAtomEventLister;
Tau_unify_object_t *mrnetAtomUnifier;

extern "C" void calculateStats(double *sum, double *sumofsqr, 
			       double *max, double *min,
			       int numThreads, int dataType,
			       FunctionInfo *fi, int ctr);

extern "C" void Tau_mon_connect() {

  //  printf("Mon Connect called\n");
  
  int size;

  PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
  PMPI_Comm_size(MPI_COMM_WORLD, &size);

  char myHostName[64];

  gethostname(myHostName, sizeof(myHostName));
  profiledir = TauEnv_get_profiledir();

  int targetRank;
  int beRank, mrnetPort, mrnetRank;
  char mrnetHostName[64];

  int in_beRank, in_mrnetPort, in_mrnetRank;
  char in_mrnetHostName[64];

  // Rank 0 reads in all connection information and sends appropriate
  //   chunks to the other ranks.
  if (rank == 0) {
    TAU_VERBOSE("Connecting to ToM\n");

    // Do not proceed until front-end has written the atomic probe file.
    char atomicFileName[512];
    sprintf(atomicFileName,"%s/ToM_FE_Atomic",profiledir);
    FILE *atomicFile;
    while ((atomicFile = fopen(atomicFileName,"r")) == NULL) {
      sleep(1);
    }
    fclose(atomicFile);

    char connectionName[512];
    sprintf(connectionName,"%s/attachBE_connections",profiledir);
    FILE *connections = fopen(connectionName,"r");
    // assume there are exactly size entries in the connection file.
    for (int i=0; i<size; i++) {
      fscanf(connections, "%d %d %d %d %s\n",
	     &targetRank, &in_beRank, &in_mrnetPort, &in_mrnetRank,
	     in_mrnetHostName);
      if (targetRank == 0) {
	beRank = in_beRank;
	mrnetPort = in_mrnetPort;
	mrnetRank = in_mrnetRank;
	strncpy(mrnetHostName, in_mrnetHostName, 64);
      } else {
	PMPI_Send(&in_beRank, 1, MPI_INT, targetRank, 0, MPI_COMM_WORLD);
	PMPI_Send(&in_mrnetPort, 1, MPI_INT, targetRank, 0, MPI_COMM_WORLD);
	PMPI_Send(&in_mrnetRank, 1, MPI_INT, targetRank, 0, MPI_COMM_WORLD);
	PMPI_Send(in_mrnetHostName, 64, MPI_CHAR, targetRank, 0, 
		 MPI_COMM_WORLD);
      }
    }
    fclose(connections);
  } else {
    MPI_Status status;
    PMPI_Recv(&beRank, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
    PMPI_Recv(&mrnetPort, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
    PMPI_Recv(&mrnetRank, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
    PMPI_Recv(mrnetHostName, 64, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);
  }

  int mrnet_argc = 6;
  char *mrnet_argv[6];

  char beRankString[10];
  char mrnetPortString[10];
  char mrnetRankString[10];

  sprintf(beRankString, "%d", beRank);
  sprintf(mrnetPortString, "%d", mrnetPort);
  sprintf(mrnetRankString, "%d", mrnetRank);

  mrnet_argv[0] = ""; // dummy process name string
  mrnet_argv[1] = mrnetHostName;
  mrnet_argv[2] = mrnetPortString;
  mrnet_argv[3] = mrnetRankString;
  mrnet_argv[4] = myHostName;
  mrnet_argv[5] = beRankString;

#ifdef MRNET_LIGHTWEIGHT
  net = Network_CreateNetworkBE(mrnet_argc, mrnet_argv);
  assert(net);
#else /* MRNET_LIGHTWEIGHT */
  net = Network::CreateNetworkBE(mrnet_argc, mrnet_argv);
#endif /* MRNET_LIGHTWEIGHT */

  int tag;
  int data;

// Do not proceed until control stream is established with front-end
#ifdef MRNET_LIGHTWEIGHT
  Packet_t *p = (Packet_t *)malloc(sizeof(Packet_t));
  Network_recv(net, &tag, p, &ctrl_stream);
  Packet_unpack(p, "%d", &data);
#else /* MRNET_LIGHTWEIGHT */
  PacketPtr p;
  net->recv(&tag, p, &ctrl_stream);
  p->unpack("%d", &data);
#endif /* MRNET_LIGHTWEIGHT */

  if (data == 1) {
    broadcastResults = true;
  } else if (data == 0) {
    broadcastResults = false;
  } else {
    fprintf(stderr,"Warning: Invalid initial control signal %d.\n",
	    data);
  }

  /* DEBUG 
  printf("[%d] Got ToM control stream. Proceeding with application code\n",
	 rank);
  */
}

// more like a "last call" for protocol action than an
//   actual disconnect call. This call should not exit unless
//   the front-end says so.
extern "C" void Tau_mon_disconnect() {
  if (rank == 0) {
    TAU_VERBOSE("Disconnecting from ToM\n");
  }
  // Tell front-end to tear down network and exit
  STREAM_FLUSHSEND_BE(ctrl_stream, TOM_CONTROL, "%d", TOM_EXIT);
}

extern "C" void protocolLoop(int *globalToLocal, int numGlobal) {

  // receive from the network so that ToM will always know which stream to
  //   respond to.
  int protocolTag;
#ifdef MRNET_LIGHTWEIGHT
  Packet_t *p = (Packet_t *)malloc(sizeof(Packet_t));
  Stream_t *stream = (Stream_t *)malloc(sizeof(Stream_t));
#else /* MRNET_LIGHTWEIGHT */
  PacketPtr p;
  Stream *stream;
#endif /* MRNET_LIGHTWEIGHT */

  bool processProtocol = true;

  // data from Basestats to be kept for later procotols. *CWL* find a
  //   modular way for this information to be exchanged between protocols.
  double *means;
  double *std_devs;  
  double *mins;
  double *maxes;

  int numThreads = RtsLayer::getNumThreads();
  int numCounters = Tau_Global_numCounters;

  while (processProtocol) {
#ifdef MRNET_LIGHTWEIGHT
    Network_recv(net, &protocolTag, p, &stream);
#else /* MRNET_LIGHTWEIGHT */
    net->recv(&protocolTag, p, &stream);
#endif /* MRNET_LIGHTWEIGHT */

    switch (protocolTag) {
    case PROT_UNIFY: {
      // Rank 0 additionally responds to the front-end with all global
      //   function name strings.
      if (rank == 0) {
	int tag;
#ifdef MRNET_LIGHTWEIGHT
	Packet_t *p = (Packet_t *)malloc(sizeof(Packet_t));    
#else /* MRNET_LIGHTWEIGHT */
	PacketPtr p;
#endif /* MRNET_LIGHTWEIGHT */
	printf("Num Global = %d\n", numGlobal);
#ifdef MRNET_LIGHTWEIGHT
	Network_recv(net, &tag, p, &stream);
#else /* MRNET_LIGHTWEIGHT */
        net->recv(&tag, p, &stream);
#endif /* MRNET_LIGHTWEIGHT */
	STREAM_FLUSHSEND_BE(stream, PROT_UNIFY, "%as",
			 mrnetFuncUnifier->globalStrings, numGlobal);
      }
      break;
    }
    case PROT_BASESTATS: {
      // *DEBUG* 
      if (rank == 0) {
	printf("BE: Instructed by FE to report events and counters.\n");
      }
      // First message is a request for names
      // Invoke Unification
      FunctionEventLister *functionEventLister = new FunctionEventLister();
      Tau_unify_object_t *functionUnifier = 
	Tau_unify_unifyEvents(functionEventLister);
      // Send Names of events.
      char **tomNames;
      if (rank == 0) {
	// send the array of event name strings.
	tomNames = (char **)malloc((numCounters+numGlobal)*sizeof(char*));
	for (int m=0; m<numCounters; m++) {
	  tomNames[m] =
	    (char *)malloc((strlen(TauMetrics_getMetricName(m))+1)*
			   sizeof(char));
	  strcpy(tomNames[m],TauMetrics_getMetricName(m));
	}
	for (int f=0; f<numGlobal; f++) {
	  tomNames[f+numCounters] = 
	    (char *)malloc((strlen(functionUnifier->globalStrings[f])+1)*
			   sizeof(char));
	  strcpy(tomNames[f+numCounters],functionUnifier->globalStrings[f]);
	}
	STREAM_FLUSHSEND_BE(stream, PROT_BASESTATS, "%d %d %as",
			    rank, numCounters, tomNames, 
			    numCounters+numGlobal);
      } else {
	// send a single null string over.
	tomNames = (char **)malloc(sizeof(char*));
	tomNames[0] = (char *)malloc(sizeof(char));
	strcpy(tomNames[0],"");
	STREAM_FLUSHSEND_BE(stream, PROT_BASESTATS, "%d %d %as",
			    rank, 0, tomNames, 1);
      }

      // Then receive request for stats. No need to unpack.
      int tag;
#ifdef MRNET_LIGHTWEIGHT
      Network_recv(net, &tag, p, &stream);
#else /* MRNET_LIGHTWEIGHT */
      net->recv(&tag, p, &stream);
#endif /* MRNET_LIGHTWEIGHT */
      assert(tag == PROT_BASESTATS);

      int numItems = numGlobal*numCounters*TOM_NUM_VALUES;

      double *out_sums;
      double *out_sumsofsqr;
      double *out_mins;
      double *out_maxes;
      out_sums = (double *)TAU_UTIL_MALLOC(numItems*sizeof(double));
      out_sumsofsqr = (double *)TAU_UTIL_MALLOC(numItems*sizeof(double));
      out_mins = (double *)TAU_UTIL_MALLOC(numItems*sizeof(double));
      out_maxes = (double *)TAU_UTIL_MALLOC(numItems*sizeof(double));

      // For each event, how many threads contribute values 
      //   from this node?
      int *threads;
      threads = (int *)TAU_UTIL_MALLOC(numGlobal*sizeof(int));

      // Construct the data arrays to be sent.
      for (int evt=0; evt<numGlobal; evt++) {
	if (globalToLocal[evt] != -1) {

	  FunctionInfo *fi = TheFunctionDB()[globalToLocal[evt]];

	  for (int ctr=0; ctr<numCounters; ctr++) {
	    // accumulate for threads but contribute as a node
	    // Write thread-accumulated values into the appropriate arrays
	    for (int i=0; i<TOM_NUM_VALUES; i++) {
	      int aIdx = getTomFlattenedIdx(numCounters, TOM_NUM_VALUES,
					    evt, ctr, i);
	      calculateStats(&out_sums[aIdx],
			     &out_sumsofsqr[aIdx],
			     &out_mins[aIdx],
			     &out_maxes[aIdx],
			     numThreads, 
			     i, fi, ctr);
	    }
	  }
	  threads[evt] = numThreads;
	} else { /* globalToLocal[evt] != -1 */
	  // send a null contribution for each counter associated with
	  //   the function for this node
	  for (int ctr=0; ctr<numCounters; ctr++) {
	    for (int i=0; i<TOM_NUM_VALUES; i++) {
	      int aIdx = getTomFlattenedIdx(numCounters, TOM_NUM_VALUES,
					    evt, ctr, i);
	      out_sums[aIdx] = 0.0;
	      out_sumsofsqr[aIdx] = 0.0;
	      out_mins[aIdx] = 0.0;
	      out_maxes[aIdx] = 0.0;
	    }
	  }
	  threads[evt] = 0;
	} /* globalToLocal[evt] != -1 */
      }

      STREAM_FLUSHSEND_BE(stream, protocolTag, 
			  "%d %d %alf %alf %alf %alf %ad %d",
			  numGlobal, numCounters,
			  out_sums, numItems,
			  out_sumsofsqr, numItems,
			  out_mins, numItems,
			  out_maxes, numItems,
			  threads, numGlobal,
			  numThreads);

      // Get results of the protocol from the front-end.
      if (broadcastResults) {
	int numMeans, numStdDevs, numMins, numMaxes;
#ifdef MRNET_LIGHTWEIGHT
	Network_recv(net, &protocolTag, p, &stream);
	Packet_unpack(p, "%alf %alf",
		      &means, &numMeans, &std_devs, &numStdDevs,
		      &mins, &numMins, &maxes, &numMaxes);
#else /* MRNET_LIGHTWEIGHT */
        net->recv(&protocolTag, p, &stream);
        p->unpack("%alf %alf",
                  &means, &numMeans, &std_devs, &numStdDevs,
                  &mins, &numMins, &maxes, &numMaxes);
#endif /* MRNET_LIGHTWEIGHT */
      }
      break;
    }
    case PROT_HIST: {
      int numBins;
      int numKeep;
      int numItems;
      int *keepItem;

      // in the case of the Histogramming Protocol, a percentage value
      //   is sent by the front-end to determine the threshold for
      //   exclusive execution time duration. Any event not satisfying
      //   the threshold will be filtered off.
#ifdef MRNET_LIGHTWEIGHT
      Packet_unpack(p, "%ad %d %d", &keepItem, &numItems, &numKeep, &numBins);
#else /* MRNET_LIGHTWEIGHT */
      p->unpack("%ad %d %d", &keepItem, &numItems, &numKeep, &numBins);
#endif /* MRNET_LIGHTWEIGHT */

      // We send a set of bins for each counter + event combo that has
      //    not been filtered out.
      int *histBins;
      histBins = new int[numBins*numKeep];
      for (int i=0; i<numBins*numKeep; i++) {
	histBins[i] = 0;
      }

      int keepIdx = 0;
      for (int ctr=0; ctr<numCounters; ctr++) {
	for (int evt=0; evt<numGlobal; evt++) {
	  int aIdx = evt*numCounters+ctr;
	  if (keepItem[aIdx] == 1) {
	    if (globalToLocal[evt] != -1) {
	      FunctionInfo *fi = TheFunctionDB()[globalToLocal[evt]];
	      double range = maxes[aIdx] - mins[aIdx];
	      double interval = range/numBins;
	      // accumulate for threads but contribute as a node
	      for (int tid=0; tid<numThreads; tid++) {
		// going to work only with exclusive values for now.
		double val = fi->getDumpExclusiveValues(tid)[ctr];
		int histIdx = (int)floor((val-mins[aIdx])/interval);
		// hackish way of dealing with rounding problems.
		if (histIdx < 0) {
		  histIdx = 0;
		}
		if (histIdx >= numBins) {
		  histIdx = numBins-1;
		}
		histBins[keepIdx*numBins+histIdx]++;
	      }
	    }
	    keepIdx++;
	  } 
	}
      }

      STREAM_FLUSHSEND_BE(stream, protocolTag, "%ad", histBins, numKeep*numBins);
      break;
    }
    case PROT_CLASSIFIER: {
      break;
    }
    case PROT_CLUST_KMEANS: {
      break;
    }
    case PROT_EXIT: {
      // Front-end is done with this round of the processing protocol.
      //   Backends are now allowed to proceed with computation.
      processProtocol = false;
      break;
    }
    default: {
      printf("Warning: Unknown protocol tag [%d]\n", protocolTag);
    }
    }
  } /* while (processProtocol) */
  if (rank == 0) {
    TAU_VERBOSE("BE: Protocol handled. Application computation follows.\n");
  }
}

extern "C" void Tau_mon_onlineDump() {
  // *DEBUG* printf("Tau Mon data ready for dump.\n");

  // Need to get data loaded and computed from the stacks
  int numThreads = RtsLayer::getNumThreads();
  for (int tid = 0; tid<numThreads; tid++) {
    TauProfiler_updateIntermediateStatistics(tid);
  }

  // Unify events
  mrnetFuncEventLister = new FunctionEventLister();
  mrnetFuncUnifier = Tau_unify_unifyEvents(mrnetFuncEventLister);
  mrnetAtomEventLister = new AtomicEventLister();
  mrnetAtomUnifier = Tau_unify_unifyEvents(mrnetAtomEventLister);

  // process events in global order, pretty much the same way Collate handles
  //   the unified data.

  // the global number of events
  int numGlobal = mrnetFuncUnifier->globalNumItems;
  int numLocal = mrnetFuncUnifier->localNumItems;
  assert(numLocal <= numGlobal);
  int *globalToLocal = 
    (int*)TAU_UTIL_MALLOC(numGlobal*sizeof(int));
  // initialize all global entries to -1
  //   where -1 indicates that the event did not occur for this rank
  for (int i=0; i<numGlobal; i++) { 
    globalToLocal[i] = -1; 
  }
  for (int i=0; i<numLocal; i++) {
    // set reverse unsorted mapping
    globalToLocal[mrnetFuncUnifier->mapping[i]] = mrnetFuncUnifier->sortMap[i];
  }
  // Tell the MRNet front-end that the data is ready and wait for
  //   protocol instructions.
  STREAM_FLUSHSEND_BE(ctrl_stream, TOM_CONTROL, "%d", PROT_DATA_READY);

  // Start the protocol loop.
  protocolLoop(globalToLocal, numGlobal);
}

void calculateStats(double *sum, double *sumofsqr, 
		    double *max, double *min,
		    int numThreads, int dataType,
		    FunctionInfo *fi, int ctr) {
  *sum = 0.0;
  *sumofsqr = 0.0;
  
  for (int tid=0; tid<numThreads; tid++) {
    double val;
    switch (dataType) {
    case TOM_VAL_INCL: {
      val = fi->getDumpInclusiveValues(tid)[ctr];
      break;
    }
    case TOM_VAL_EXCL: {
      val = fi->getDumpExclusiveValues(tid)[ctr];
      break;
    }
    case TOM_VAL_CALL: {
      val = fi->GetCalls(tid)*1.0;
      break;
    }
    case TOM_VAL_SUBR: {
      val = fi->GetSubrs(tid)*1.0;
      break;
    }
    default: {
      // *CWL* Do something about this error condition.
    }
    };

    *sum += val;
    *sumofsqr += val*val;
    if (tid == 0) {
      *min = val;
      *max = val;
    } else {
      if (*min > val) {
	*min = val;
      }
      if (*max < val) {
	*max = val;
      }
    }
  }
}

#ifdef MRNET_LIGHTWEIGHT
} /* extern "C" */
#endif /* MRNET_LIGHTWEIGHT */

#endif /* TAU_EXP_UNIFY */
#endif /* TAU_MONITORING */
