/****************************************************************************
 **			TAU Portable Profiling Package			   **
 **			http://www.cs.uoregon.edu/research/tau	           **
 *****************************************************************************
 **    Copyright 1997-2009					   	   **
 **    Department of Computer and Information Science, University of Oregon **
 **    Advanced Computing Laboratory, Los Alamos National Laboratory        **
 ****************************************************************************/
/***************************************************************************
 **	File 		: UserEvent.h					  **
 **	Description 	: TAU Profiling Package				  **
 *	Contact		: tau-team@cs.uoregon.edu 		 	  **
 **	Documentation	: See http://www.cs.uoregon.edu/research/tau      **
 ***************************************************************************/

#ifndef _USEREVENT_H_
#define _USEREVENT_H_

#include <string>
#include <limits>
#include <vector>
#include <map>
#include <Profile/TauInit.h>
#include <Profile/TauEnv.h>


namespace tau {
//=============================================================================

// Forward declaration
class Profiler;

//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
typedef double TAU_EVENT_DATATYPE;


//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
class TauUserEvent
{
public:

  struct Data
  {
    Data() :
      minVal(std::numeric_limits<TAU_EVENT_DATATYPE>::max()),
      maxVal(-std::numeric_limits<TAU_EVENT_DATATYPE>::max()),
      sumVal(0), sumSqrVal(0), lastVal(0), userVal(0), nEvents(0)
    { }

    TAU_EVENT_DATATYPE minVal;
    TAU_EVENT_DATATYPE maxVal;
    TAU_EVENT_DATATYPE sumVal;
    TAU_EVENT_DATATYPE sumSqrVal;
    TAU_EVENT_DATATYPE lastVal;
    TAU_EVENT_DATATYPE userVal;
    size_t nEvents;
  };

  static void ReportStatistics(bool ForEachThread=false);

public:

  TauUserEvent() :
      eventId(0), name("No Name"),
      minEnabled(true), maxEnabled(true), meanEnabled(true),
      stdDevEnabled(true), monoIncreasing(false)
  {
    AddEventToDB();
  }

  TauUserEvent(TauUserEvent const & e) :
      eventId(0), name(e.name),
      minEnabled(e.minEnabled), maxEnabled(e.maxEnabled),
      meanEnabled(e.meanEnabled), stdDevEnabled(e.stdDevEnabled),
      monoIncreasing(e.monoIncreasing)
  {
    AddEventToDB();
  }

  TauUserEvent(std::string const & name, bool increasing=false) :
      eventId(0), name(name), minEnabled(true), maxEnabled(true),
      meanEnabled(true), stdDevEnabled(true), monoIncreasing(increasing)
  {
    AddEventToDB();
  }

  TauUserEvent(char const * name, bool increasing=false) :
      eventId(0), name(name), minEnabled(true), maxEnabled(true),
      meanEnabled(true), stdDevEnabled(true), monoIncreasing(increasing)
  {
    AddEventToDB();
  }

  ~TauUserEvent(void) {
    Tau_destructor_trigger();
  }

  TauUserEvent & operator=(const TauUserEvent & e) {
    // Why isn't eventId copied?
    name = e.name;
    minEnabled = e.minEnabled;
    maxEnabled = e.maxEnabled;
    meanEnabled = e.meanEnabled;
    stdDevEnabled = e.stdDevEnabled;
    return *this;
  }

  x_uint64 GetId(void) const {
    return eventId;
  }

  std::string const & GetName(void) const {
    return name;
  }
  void SetName(std::string const & value) {
    name = value;
  }

  bool IsMinEnabled(void) const {
    return minEnabled;
  }
  void SetMinEnabled(bool value) {
    minEnabled = value;
  }

  bool IsMaxEnabled(void) const {
    return maxEnabled;
  }
  void SetMaxEnabled(bool value) {
    maxEnabled = value;
  }

  bool IsMeanEnabled(void) const {
    return meanEnabled;
  }
  void SetMeanEnabled(bool value) {
    meanEnabled = value;
  }

  bool IsStdDevEnabled(void) const {
    return stdDevEnabled;
  }
  void SetStdDevEnabled(bool value) {
    stdDevEnabled = value;
  }

  bool IsMonotonicallyIncreasing(void) const {
    return monoIncreasing;
  }
  void SetMonotonicallyIncreasing(bool value) {
    monoIncreasing = value;
  }

  TAU_EVENT_DATATYPE GetMin(void) {
    Data const & d = MyThreadData();
    return d.nEvents ? d.minVal : 0;
  }
  TAU_EVENT_DATATYPE GetMin(int tid) {
    Data const & d = ThreadData(tid);
    return d.nEvents ? d.minVal : 0;
  }

  TAU_EVENT_DATATYPE GetMax(void) {
    Data const & d = MyThreadData();
    return d.nEvents ? d.maxVal : 0;
  }
  TAU_EVENT_DATATYPE GetMax(int tid) {
    Data const & d = ThreadData(tid);
    return d.nEvents ? d.maxVal : 0;
  }

  TAU_EVENT_DATATYPE GetSum(void) {
    return MyThreadData().sumVal;
  }
  TAU_EVENT_DATATYPE GetSum(int tid) {
    return ThreadData(tid).sumVal;
  }

  TAU_EVENT_DATATYPE GetSumSqr(void) {
    return MyThreadData().sumSqrVal;
  }
  TAU_EVENT_DATATYPE GetSumSqr(int tid) {
    return ThreadData(tid).sumSqrVal;
  }

  TAU_EVENT_DATATYPE GetMean(void) {
    Data const & d = MyThreadData();
    return d.nEvents ? (d.sumVal / d.nEvents) : 0;
  }
  TAU_EVENT_DATATYPE GetMean(int tid) {
    Data const & d = ThreadData(tid);
    return d.nEvents ? (d.sumVal / d.nEvents) : 0;
  }

  size_t GetNumEvents(void) {
    return MyThreadData().nEvents;
  }
  size_t GetNumEvents(int tid) {
    return ThreadData(tid).nEvents;
  }

  void ResetData(void) {
    MyThreadData() = Data();
  }
  void ResetData(int tid) {
    ThreadData(tid) = Data();
  }

  void TriggerEvent(TAU_EVENT_DATATYPE data) {
    TriggerEvent(data, RtsLayer::myThread(), 0, 0);
  }
  void TriggerEvent(TAU_EVENT_DATATYPE data, int tid) {
    TriggerEvent(data, tid, 0, 0);
  }
  void TriggerEvent(TAU_EVENT_DATATYPE data, int tid, double timestamp, int use_ts);

private:

  Data & MyThreadData() {
    return eventData[RtsLayer::myThread()];
  }

  Data & ThreadData(int tid) {
    return eventData[tid];
  }

  void AddEventToDB();

  Data eventData[TAU_MAX_THREADS];

  x_uint64 eventId;
  std::string name;
  bool minEnabled;
  bool maxEnabled;
  bool meanEnabled;
  bool stdDevEnabled;
  bool monoIncreasing;
};


//////////////////////////////////////////////////////////////////////
// Don't inherit TauUserEvent to avoid virtual function overhead
//////////////////////////////////////////////////////////////////////
class TauContextUserEvent
{
public:

  TauContextUserEvent(char const * name, bool monoIncr=false) :
      userEvent(new TauUserEvent(name, monoIncr)),
#ifdef TAU_SCOREP
      contextEnabled(true)
#else
      contextEnabled(TauEnv_get_callpath_depth() != 0)
#endif
  { }

  void SetContextEnabled(bool value) {
    contextEnabled = value;
  }

  std::string const & GetName() const {
    return userEvent->GetName();
  }

  void TriggerEvent(TAU_EVENT_DATATYPE data) {
    TriggerEvent(data, RtsLayer::myThread(), 0, 0);
  }
  void TriggerEvent(TAU_EVENT_DATATYPE data, int tid) {
    TriggerEvent(data, tid, 0, 0);
  }
  void TriggerEvent(TAU_EVENT_DATATYPE data, int tid, double timestamp, int use_ts);

private:

  long * FormulateContextComparisonArray(Profiler * current);
  std::string FormulateContextNameString(Profiler * current);

  bool contextEnabled;
  TauUserEvent * userEvent;

};


//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
struct AtomicEventDB : public std::vector<TauUserEvent*>
{
  AtomicEventDB() {
    Tau_init_initializeTAU();
  }
  ~AtomicEventDB() {
    Tau_destructor_trigger();
  }
};

AtomicEventDB & TheEventDB(void);


//=============================================================================
} // END namespace tau


#endif /* _USEREVENT_H_ */

/***************************************************************************
 * $RCSfile: UserEvent.h,v $   $Author: amorris $
 * $Revision: 1.17 $   $Date: 2009/09/28 18:28:48 $
 * POOMA_VERSION_ID: $Id: UserEvent.h,v 1.17 2009/09/28 18:28:48 amorris Exp $ 
 ***************************************************************************/
