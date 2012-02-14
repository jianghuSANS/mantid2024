/*WIKI*

Filter events for VULCAN

*WIKI*/
//----------------------------------------------------------------------
// Includes
//----------------------------------------------------------------------

#include "MantidAlgorithms/FilterEventsHighFrequency.h"
#include "MantidKernel/System.h"
#include "MantidAPI/FileProperty.h"
#include "MantidAPI/WorkspaceProperty.h"
#include "MantidAPI/IEventList.h"
#include "MantidDataObjects/EventList.h"
#include "MantidDataObjects/Events.h"
#include "MantidAPI/WorkspaceProperty.h"
#include "MantidKernel/UnitFactory.h"
#include "MantidGeometry/Instrument.h"
#include "MantidKernel/TimeSeriesProperty.h"
#include <algorithm>
#include <fstream>

using namespace Mantid::Kernel;
using namespace Mantid::API;

namespace Mantid
{
namespace Algorithms
{

  DECLARE_ALGORITHM(FilterEventsHighFrequency)

  //----------------------------------------------------------------------------------------------
  /** Constructor
   */
  FilterEventsHighFrequency::FilterEventsHighFrequency()
  {
  }
    
  //----------------------------------------------------------------------------------------------
  /** Destructor
   */
  FilterEventsHighFrequency::~FilterEventsHighFrequency()
  {
  }

  void FilterEventsHighFrequency::initDocs(){

    return;
  }

  /*
   * Declare input/output properties
   */
  void FilterEventsHighFrequency::init(){

    this->declareProperty(new API::WorkspaceProperty<DataObjects::EventWorkspace>("InputEventWorkspace", "", Direction::InOut),
        "Input EventWorkspace.  Each spectrum corresponds to 1 pixel");
    this->declareProperty(new API::WorkspaceProperty<DataObjects::EventWorkspace>("OutputWorkspace", "Anonymous", Direction::Output),
        "Output EventWorkspace.");

    this->declareProperty("LogName", "", "Log's name to filter events.");

    this->declareProperty(new API::FileProperty("InputCalFile", "", API::FileProperty::Load, ".dat"),
        "Input pixel TOF calibration file in column data format");

    this->declareProperty("SensorToSampleOffset", 0.0, "Offset in micro-second from sample to sample environment sensor");
    this->declareProperty("ValueLowerBoundary", 0.0, "Lower boundary of sample environment value for selected events");
    this->declareProperty("ValueUpperBoundary", 0.0, "Upper boundary of sample environment value for selected events");

    std::vector<std::string> timeoptions;
    timeoptions.push_back("Absolute Time (nano second)");
    timeoptions.push_back("Relative Time (second)");
    timeoptions.push_back("Percentage");
    this->declareProperty("TimeRangeOption", "Relative Time (second)", new ListValidator(timeoptions),
        "User defined time range (T0, Tf) is of absolute time (second). ");
    this->declareProperty("T0", 0.0, "Earliest time of the events to be selected.  It can be absolute time (ns), relative time (second) or percentage.");
    this->declareProperty("Tf", 100.0, "Latest time of the events to be selected.  It can be absolute time (ns), relative time (second) or percentage.");

    this->declareProperty("WorkspaceIndex", -1, "The index of the workspace to have its events filtered. ");
    this->declareProperty("NumberOfIntervals", 1, "Number of even intervals in the selected region. ");

    this->declareProperty("NumberOfWriteOutEvents", 1000,
        "Number of events filtered to be written in output file for debug.");

    this->declareProperty(new API::FileProperty("OutputDirectory", "", API::FileProperty::OptionalDirectory),
        "Directory of all output files");

    return;
  }

  /*
   * Main body to execute the algorithm
   * Conventions:
   * (1) All time should be converted to absolute time in nano-second during calculation
   * (2) Output can be more flexible
   */
  void FilterEventsHighFrequency::exec(){

    // 0. Init
    mNumMissFire = 0;

    // 1. Get property
    eventWS = this->getProperty("InputEventWorkspace");
    const std::string outputdir = this->getProperty("OutputDirectory");

    const std::string calfilename = this->getProperty("InputCalFile");
    double tempoffset = this->getProperty("SensorToSampleOffset");
    mSensorSampleOffset = static_cast<int64_t>(tempoffset*1000);

    mLowerLimit = this->getProperty("ValueLowerBoundary");
    mUpperLimit = this->getProperty("ValueUpperBoundary");

    std::string logname = this->getProperty("LogName");

    int itemp = this->getProperty("WorkspaceIndex");

    if (itemp < 0)
    {
      filterSingleSpectrum = false;
      wkspIndexToFilter = 0;
    }
    else
    {
      filterSingleSpectrum = true;
      wkspIndexToFilter = static_cast<size_t>(itemp);
    }

    mFilterIntervals = getProperty("NumberOfIntervals");
    if (mFilterIntervals <= 0)
    {
      g_log.error() << "Number of filter intervals (windows) cannot be less of equal to 0.  Input = " << mFilterIntervals << std::endl;
      throw std::invalid_argument("Non-positive number of filter intervals is not allowed.");
    }
    numOutputEvents = getProperty("NumberOfWriteOutEvents");

    // b) Some time issues
    double t0r, tfr;
    t0r = this->getProperty("T0");
    tfr = this->getProperty("Tf");
    if (t0r >= tfr){
      g_log.error() << "User defined filter starting time (T0 = " << t0r << ") is later than ending time (Tf = " << tfr << ")" << std::endl;
      throw std::invalid_argument("User input T0 and Tf error!");
    }
    std::string timeoption = this->getProperty("TimeRangeOption");

    const API::Run& runlog = eventWS->run();
    std::string runstartstr = runlog.getProperty("run_start")->value();
    Kernel::DateAndTime runstart(runstartstr);
    mRunStartTime = runstart;

    if (timeoption.compare("Absolute Time (nano second)")==0){
      // i. absolute time
      mFilterT0 = Kernel::DateAndTime(static_cast<int64_t>(t0r));
      mFilterTf = Kernel::DateAndTime(static_cast<int64_t>(tfr));
    }
    else if (timeoption.compare("Relative Time (second)") == 0){
      // ii. relative time
      mFilterT0 = runstart + t0r;
      mFilterTf = runstart + tfr;
    }
    else{
      // iii. percentage
      Kernel::TimeSeriesProperty<double>* tlog = dynamic_cast<Kernel::TimeSeriesProperty<double>* >(
          eventWS->run().getProperty(logname));
      if (!tlog){
        g_log.error() << "TimeSeriesProperty Log " << logname << " does not exist in workspace " <<
            eventWS->getName() << std::endl;
        throw std::invalid_argument("TimeSeriesProperty log cannot be found");
      }

      if (t0r < 0.0){
        t0r = 0.0;
        g_log.warning() << "For percentage T0 cannot be less than 0.  Auto-reset to 0.0 percent." << std::endl;
      }
      if (tfr > 100.0){
        tfr = 100.0;
        g_log.warning() << "For percentage Tf cannot be larger than 100.  Auto-reset to 100 percent." << std::endl;
      }

      std::vector<Kernel::DateAndTime> times = tlog->timesAsVector();
      int64_t ts = times[0].total_nanoseconds();
      int64_t te = times[times.size()-1].total_nanoseconds();
      mFilterT0 = times[0] + static_cast<int64_t>(static_cast<double>(te-ts)*t0r*0.01);
      mFilterTf = times[0] + static_cast<int64_t>(static_cast<double>(te-ts)*tfr*0.01);
    }

    g_log.information() << "User Filter:  T0 = " << mFilterT0 << ";  Tf = " << mFilterTf << std::endl;

    // 2. Check and process input
    // a) Event Workspace
    for (size_t i = 0; i < eventWS->getNumberHistograms(); i ++){
      const DataObjects::EventList events = eventWS->getEventList(i);
      std::set<detid_t> detids = events.getDetectorIDs();
      if (detids.size() != 1){
        g_log.error() << "Spectrum " << i << " has more than 1 detectors (" << detids.size() << "). Algorithm does not support! " << std::endl;
        throw std::invalid_argument("EventWorkspace error");
      }
    }

    // c) Sample environment workspace:  increment workspace?  If log file is given, then read from log file and ignore the workspace
    if (!logname.empty())
    {
      g_log.notice() << "Using input EventWorkspace's log " << logname << std::endl;
      this->processTimeLog(logname);
    } else
    {
      g_log.error() << "Log name is not give!" << std::endl;
      throw std::invalid_argument("Log name is not given!");
    }

    // 3. Read calibration file
    importCalibrationFile(calfilename);

    // 4. Build new Workspace
    createEventWorkspace();

    // 5. Filter
    filterEvents();

    // 6. Set output
    g_log.debug() << "Trying to set Output Workspace: " << outputWS->getName() << std::endl;
    this->setProperty("OutputWorkspace", outputWS);
    g_log.debug() << "Output Workspace is set!" << " Number of Events in Spectrum 0 = " << outputWS->getEventList(0).getNumberEvents() << std::endl;

    return;
  } // exec


  /*
   * Convert time log to vectors for fast access
   */
  void FilterEventsHighFrequency::processTimeLog(std::string logname){

    g_log.information() << "Starting processTimeLog()" << std::endl;

    // 1. Get Log
    const API::Run& runlogs = eventWS->run();
    Kernel::TimeSeriesProperty<double> * fastfreqlog
        = dynamic_cast<Kernel::TimeSeriesProperty<double> *>( runlogs.getLogData(logname) );

    // 2. Transfer to mSETimes (nanoseconds)
    std::vector<Kernel::DateAndTime> timevec = fastfreqlog->timesAsVector();

    // a) Index = 0 case
    mSETimes.push_back(timevec[0].total_nanoseconds());
    double tv = fastfreqlog->getSingleValue(timevec[0]);
    mSEValues.push_back(tv);

    // b) Index > 0 case: need to take care of duplicate log entry
    size_t numduplicates = 0;
    size_t numreversed = 0;
    std::stringstream errss;

    for (size_t i = 1; i < timevec.size(); i ++){
      if (timevec[i] > timevec[i-1])
      {
        // Normal case
        mSETimes.push_back(timevec[i].total_nanoseconds());
        double tv = fastfreqlog->getSingleValue(timevec[i]);
        mSEValues.push_back(tv);
      }
      else if (timevec[i] == timevec[i-1])
      {
        // Duplicate case
        numduplicates += 1;
        int64_t dt = timevec[i].total_nanoseconds()-timevec[i-1].total_nanoseconds();
        errss << "Time [" << i << "] = "
            << timevec[i] << " is duplicated with previous time "
            << timevec[i-1] << ".  dT = " << dt << std::endl;
      }
      else
      {
        // Reversed order case
        // Duplicate case
        numreversed += 1;
        int64_t dt = timevec[i].total_nanoseconds()-timevec[i-1].total_nanoseconds();
        errss << "Time [" << i << "] = "
            << timevec[i] << " is earlier than previous time "
            << timevec[i-1] << ".  dT = " << dt << std::endl;
      }
    }

    // 3. Output Error Message
    if (numduplicates + numreversed > 0)
    {
      std::string errmsg = errss.str();
      g_log.debug() << "Log Error Message: " << std::endl << errmsg;
      g_log.error() << "Log Information: " << std::endl
          << "  Number of duplicates =  " << numduplicates
          << "  Number of reversed = " << numreversed << std::endl
          << "  Original Log Size = " << timevec.size() << "  Cleaned Log Size = " << mSETimes.size() << std::endl;
    }

    g_log.information() << "Finished processTimeLog()" << std::endl;
    return;
  }

  /*
   * Import TOF calibration/offset file for each pixel.
   */
  void FilterEventsHighFrequency::importCalibrationFile(std::string calfilename){

    detid_t indet;
    double doffset; // Assuming the file gives offset in micro-second

    // 1. Check workspace
    if (!eventWS){
      g_log.error() << "Required to import EventWorkspace before calling importCalibrationFile()" << std::endl;
      throw std::invalid_argument("Calling function in wrong order!");
    }

    // 2. Open file
    std::ifstream ifs;
    mCalibDetectorIDs.clear();
    mCalibOffsets.clear();

    try{
      // a. Successful scenario
      ifs.open(calfilename.c_str(), std::ios::in);

      for (size_t i = 0; i < eventWS->getNumberHistograms(); i ++){
        // i. each pixel:  get detector ID from EventWorkspace
        const DataObjects::EventList events = this->eventWS->getEventList(i);
        std::set<detid_t> detids = events.getDetectorIDs();
        std::set<detid_t>::iterator detit;
        detid_t detid = 0;
        for (detit=detids.begin(); detit!=detids.end(); ++detit)
          detid = *detit;

        // ii. read file
        ifs >> indet >> doffset;

        // iii. store
        if (indet != detid){
          g_log.error() << "Error!  Line " << i << " Should read in pixel " << detid << "  but read in " << indet << std::endl;
        }
        if (doffset < 0 || doffset > 1.0){
          g_log.error() << "Error!  Line " << i << " have ratio offset outside (0,1) " << detid << "  but read in " << indet << std::endl;
        }

        mCalibDetectorIDs.push_back(detid);
        mCalibOffsets.push_back(doffset);
      }

      ifs.close();

    } catch (std::ifstream::failure&){
      // b. Using faking offset/calibration
      g_log.error() << "Open calibration/offset file " << calfilename << " error " << std::endl;
      g_log.notice() << "Using default detector offset/calibration" << std::endl;

      // Reset vectors
      mCalibDetectorIDs.clear();
      mCalibOffsets.clear();

      for (size_t i = 0; i < eventWS->getNumberHistograms(); i ++){
        const DataObjects::EventList events = this->eventWS->getEventList(i);
        std::set<detid_t> detids = events.getDetectorIDs();
        std::set<detid_t>::iterator detit;
        detid_t detid = 0;
        for (detit=detids.begin(); detit!=detids.end(); ++detit)
          detid = *detit;

        mCalibDetectorIDs.push_back(detid);
        mCalibOffsets.push_back(1.0);
      }

    } // try-catch

    return;
  }

  /*
   * Create an output EventWorkspace w/o any events
   */
  void FilterEventsHighFrequency::createEventWorkspace(){

    // 1. Initialize:use dummy numbers for arguments, for event workspace it doesn't matter
    outputWS = DataObjects::EventWorkspace_sptr(new DataObjects::EventWorkspace());
    outputWS->setName("FilteredWorkspace");
    outputWS->initialize(1,1,1);

    // 2. Set the units
    outputWS->getAxis(0)->unit() = UnitFactory::Instance().create("TOF");
    outputWS->setYUnit("Counts");
    // TODO: Give a meaningful title later
    outputWS->setTitle("Filtered");

    // 3. Add the run_start property:
    int runnumber = eventWS->getRunNumber();
    outputWS->mutableRun().addProperty("run_number", runnumber);

    std::string runstartstr = eventWS->run().getProperty("run_start")->value();
    outputWS->mutableRun().addProperty("run_start", runstartstr);

    // 4. Instrument
    IAlgorithm_sptr loadInst= createSubAlgorithm("LoadInstrument");
    // Now execute the sub-algorithm. Catch and log any error, but don't stop.
    loadInst->setPropertyValue("InstrumentName", eventWS->getInstrument()->getName());
    loadInst->setProperty<MatrixWorkspace_sptr> ("Workspace", outputWS);
    loadInst->setProperty("RewriteSpectraMap", true);
    loadInst->executeAsSubAlg();
    // Populate the instrument parameters in this workspace - this works around a bug
    outputWS->populateInstrumentParameters();

    // 5. ??? Is pixel mapping file essential???

    // 6. Build spectrum and event list
    // a) We want to pad out empty pixels.
    detid2det_map detector_map;
    outputWS->getInstrument()->getDetectors(detector_map);

    g_log.debug() << "VZ: 6a) detector map size = " << detector_map.size() << std::endl;

    // b) determine maximum pixel id
    detid2det_map::iterator it;
    detid_t detid_max = 0; // seems like a safe lower bound
    for (it = detector_map.begin(); it != detector_map.end(); ++it)
      if (it->first > detid_max)
        detid_max = it->first;

    // c) Pad all the pixels and Set to zero
    std::vector<std::size_t> pixel_to_wkspindex;
    pixel_to_wkspindex.reserve(detid_max+1); //starting at zero up to and including detid_max
    pixel_to_wkspindex.assign(detid_max+1, 0);
    size_t workspaceIndex = 0;
    for (it = detector_map.begin(); it != detector_map.end(); ++it)
    {
      if (!it->second->isMonitor())
      {
        pixel_to_wkspindex[it->first] = workspaceIndex;
        DataObjects::EventList & spec = outputWS->getOrAddEventList(workspaceIndex);
        spec.addDetectorID(it->first);
        // Start the spectrum number at 1
        spec.setSpectrumNo(specid_t(workspaceIndex+1));
        workspaceIndex += 1;
      }
    }
    outputWS->doneAddingEventLists();

    // Clear
    pixel_to_wkspindex.clear();

    g_log.debug() << "VZ (End of createEventWorkspace): Total spectrum number = " << outputWS->getNumberHistograms() << std::endl;

    return;

  }

  /*
   * Filter events from eventWS to outputWS
   */
  void FilterEventsHighFrequency::filterEvents(){

    g_log.debug() << "Starting filterEvents()" << std::endl;

    shortest_tof = 1.0E10;
    longest_tof = -1;

    // 1. Sort the workspace (event) in the order absolute time
    API::IAlgorithm_sptr sort1 = createSubAlgorithm("SortEvents");
    sort1->initialize();
    sort1->setProperty("InputWorkspace", eventWS);
    sort1->setProperty("SortBy", "Pulse Time + TOF");
    sort1->execute();

    g_log.information() << "Calibration Offset Size = " << mCalibOffsets.size() << std::endl;

    // 2. Filter by each spectrum
    numoverupperbound = 0;
    numoverlowerbound = 0;
    numnegtofs = 0;
    numreversedevents = 0;
    numreasonunknown = 0;

    if (filterSingleSpectrum)
    {
      filterSingleDetectorSequential(wkspIndexToFilter);
    }
    else
    {
      for (size_t ip=0; ip<eventWS->getNumberHistograms(); ip++){
        filterSingleDetectorParallel(ip);
      } // ENDFOR: each spectrum
    }
    // PARALLEL_CHECK_INTERUPT_REGION

    // 4. Add a dummy histogramming
    //    create a default X-vector for histogramming, with just 2 bins.
    Kernel::cow_ptr<MantidVec> axis;
    MantidVec& xRef = axis.access();
    xRef.resize(2);
    xRef[0] = shortest_tof - 1; //Just to make sure the bins hold it all
    xRef[1] = longest_tof + 1;
    outputWS->setAllX(axis);

    // 5. Information output
    writeLog();

    return;
  }

  /*
   * Write out filtering summary to log
   */
  void FilterEventsHighFrequency::writeLog()
  {
    if (mNumMissFire > 0){
      g_log.error() << "Total " << mNumMissFire << " searches fall out of search range" << std::endl
          << "Number of search over lower bound  = " << numoverlowerbound << std::endl
          << "Number of search over upper bound  = " << numoverupperbound << std::endl
          << "Number of negative TOF             = " << numnegtofs << std::endl
          << "NUmber of events in reversed order = " << numreversedevents << std::endl
          << "NUmber of unknown reasons          = " << numreasonunknown << std::endl;
    }
    g_log.debug() << "End of filterEvents()" << std::endl;

    return;
  }

  /*
   * Filter events on one detector (in parallel)
   */
  void FilterEventsHighFrequency::filterSingleDetectorParallel(size_t wkspindex)
  {

    // TODO  Need to add parallel flag here!
    g_log.warning() << "This algorithm has not been implemented as parallel algorithm yet!" << std::endl;

    // For each spectrum
    // a. Offset
    double percentageoffsettof = mCalibOffsets[wkspindex];

    // b. Get all events
    DataObjects::EventList events = eventWS->getEventList(wkspindex);
    std::vector<int64_t>::iterator abstimeit;
    std::vector<DataObjects::TofEvent> newevents;

    // c. Filter the event
    size_t posoffsetL = 0;
    size_t posoffsetU = 0;
    size_t indexL = 0;
    size_t indexU = events.getNumberEvents()-1;
    bool islow = true;
    int64_t prevtime1 = 0;
    int64_t prevtime2 = 0;

    for (size_t iv=0; iv<events.getNumberEvents(); iv++){
      // FOR each event
      // 0. Determine index
      size_t index;

      if (islow){
        index = indexL;
        indexL ++;
      } else {
        index = indexU;
        indexU --;
      }
      DataObjects::TofEvent rawevent = events.getEvent(index);

      // i.  Check negative TOF, and update loop variables
      if (rawevent.tof() < 0)
      {
        PARALLEL_CRITICAL(p1)
        {
          numnegtofs += 1;
          g_log.error() << "Event " << iv << " has negative TOF " << rawevent.tof() << std::endl;
        }

        islow = !islow;
        int64_t temp = prevtime2;
        prevtime2 = prevtime1;
        prevtime1 = temp;
        continue;
      }

      // ii.  Get raw event & time: Total time = pulse time (ns) + TOF*offset - sensor-sample-offset
      int64_t mtime = rawevent.m_pulsetime.total_nanoseconds()+
          static_cast<int64_t>(rawevent.m_tof*1000*percentageoffsettof)-
          mSensorSampleOffset;

      // iii. Filter out if time falls out of (T0, Tf), and update loop variables
      if (mtime < mFilterT0.total_nanoseconds() || mtime > mFilterTf.total_nanoseconds()){
        islow = !islow;
        prevtime2 = prevtime1;
        prevtime1 = mtime;
        continue;
      }

      // iv.  Search... need to consider more situation as outside of boundary, on the grid and etc
      abstimeit = std::lower_bound(mSETimes.begin()+posoffsetL, mSETimes.end()-posoffsetU, mtime);
      size_t mindex;
      if (*abstimeit == mtime){
        // (1) On the grid
        mindex = size_t(abstimeit-mSETimes.begin());
      }
      else if (abstimeit == mSETimes.begin()){
        // (2) On first grid or out of lower bound
        mindex = size_t(abstimeit-mSETimes.begin());
      } else {
        mindex = size_t(abstimeit-mSETimes.begin())-1;
      }

      // v.   Check Result: In very rare case, events are not in absolute time's ascending order
      bool check2ndtime = false;
      if ((mtime >= mSETimes[0] && mtime < mSETimes[mSETimes.size()-1]) && (mtime < mSETimes[mindex] || mtime >= mSETimes[mindex+1]) )
      {
        check2ndtime = true;

        PARALLEL_CRITICAL(p2)
        {
          size_t numsetimes = mSETimes.size();
          if (mSETimes[numsetimes-1-posoffsetU]-mtime < 0){
            numoverupperbound += 1;
          }
          if (mtime - mSETimes[posoffsetL] < 0){
            numoverlowerbound += 1;
          }
          mNumMissFire += 1;
        }

        if (mtime < prevtime2)
        {
          // case 1:  absolute time is not in order.  do search again
          PARALLEL_CRITICAL(p3)
          {
            numreversedevents += 1;
          }

          abstimeit = std::lower_bound(mSETimes.begin(), mSETimes.end()-posoffsetU, mtime);
          if (*abstimeit == mtime){
            // (1) On the grid
            mindex = size_t(abstimeit-mSETimes.begin());
          }
          else if (abstimeit == mSETimes.begin()){
            // (2) On first grid or out of lower bound
            mindex = size_t(abstimeit-mSETimes.begin());
          } else {
            mindex = size_t(abstimeit-mSETimes.begin())-1;
          }

        }
        else
        {
          // case 2:  no idea why this happens
          PARALLEL_CRITICAL(p4)
          {
            numreasonunknown += 1;
          }
        }
      } // If-Error

      // vi.  Check 2 (Usually won't happen)
      if (mindex >= mSETimes.size()){
        size_t numsetimes = mSETimes.size();
        int64_t dt = mtime - mRunStartTime.total_nanoseconds();
        g_log.error() << "Locate " << mtime << "  Time 0 = " << mSETimes[0] << ", Time f = " << mSETimes[numsetimes-1] << std::endl;
        g_log.error() << "Time = " << mtime << "  T-T0  = " << (static_cast<double>(dt)*1.0E-9) << " sec" << std::endl;
        throw std::invalid_argument("Flag 1616:  Wrong in searching.  Out of log boundary!!!");
      }

      // vii.  Last check and output
      if (check2ndtime)
      {
        if ((mtime >= mSETimes[0] && mtime < mSETimes[mSETimes.size()-1]) && (mtime < mSETimes[mindex] || mtime >= mSETimes[mindex+1]) ){

           size_t numsetimes = mSETimes.size();
           std::stringstream errmsg;

           // (a) general information
           errmsg << "Try to locate time: " << mtime << ";  Found value = " << mSETimes[mindex] << " (@ Index = " << mindex << "), " <<
               mSETimes[mindex+1] << std::endl;
           errmsg << "Search Range   Low: " << mSETimes[posoffsetL] << "(" <<  posoffsetL << "), Diff = "
               << (mtime - mSETimes[posoffsetL]) << std::endl;
           errmsg << "                Up: " << mSETimes[numsetimes-1-posoffsetU] << "(" << posoffsetU << "), Diff = "
               << (mSETimes[numsetimes-1-posoffsetU]-mtime) << std::endl;

           // (b) compare with the previous event
           if (index != 0 && index != numsetimes-1)
           {
             size_t preindex;
             if (islow)
             {
               preindex = index-1;
             } else
             {
               preindex = index+1;
             }
             DataObjects::TofEvent preevent = events.getEvent(preindex);
             int64_t currabstime = rawevent.pulseTime().total_nanoseconds()+static_cast<int64_t>(rawevent.tof()*1000);
             int64_t prevabstime = preevent.pulseTime().total_nanoseconds()+static_cast<int64_t>(preevent.tof()*1000);
             errmsg << "Pulse Time(prev, curr):  " << preevent.pulseTime() << " , " << rawevent.pulseTime() << std::endl;
             errmsg << "TOF       (prev, curr):  " << preevent.tof() << " , " << rawevent.tof() << std::endl;
             errmsg << "Raw Time              :  " << prevabstime << ", " << currabstime << std::endl;
             errmsg << "Corrected Tiem        :  " << prevtime2 << ", " << mtime << std::endl;
             errmsg << "Real      Time Diff (curr-prev) = " << currabstime-prevabstime << std::endl;
             errmsg << "Corrected Time Diff (curr-prev) = " << mtime-prevtime2 << std::endl;
           }

           g_log.error() << errmsg.str();
         } // Exception caught!
      } // 2ND Time Check

      // viii. Filter in/out?
      double msevalue = mSEValues[mindex];
      if (msevalue >= mLowerLimit && msevalue <= mUpperLimit){
        DataObjects::TofEvent newevent(rawevent.m_tof, rawevent.m_pulsetime);
        newevents.push_back(newevent);
      }

      // ix.  Update position offset
      if (islow){
        // Update lower side offset
        posoffsetL = mindex;
      } else {
        // Update upper side offset
        if (mindex < events.getNumberEvents()){
          posoffsetU = events.getNumberEvents()-mindex-1;
        } else {
          posoffsetU = 0;
        }
      }

      islow = !islow;

      // vi. Update previous time
      prevtime2 = prevtime1;
      prevtime1 = mtime;
    } // ENDFOR iv: each event

    // 3. Add to outputWS
    DataObjects::EventList* neweventlist;
    PARALLEL_CRITICAL(p5)
    {
      neweventlist = outputWS->getEventListPtr(wkspindex);
    }

    double local_longest_tof = 0;
    double local_shortest_tof = 1.0E10;

    for (size_t iv=0; iv<newevents.size(); iv++){
      neweventlist->addEventQuickly(newevents[iv]);
      if (newevents[iv].m_tof > local_longest_tof)
      {
        local_longest_tof = newevents[iv].m_tof;
      }
      else if (newevents[iv].m_tof < local_shortest_tof)
      {
        local_shortest_tof = newevents[iv].m_tof;
      }
    } // ENDFOR iv

    PARALLEL_CRITICAL(p6)
    {
      if (local_longest_tof > longest_tof)
        longest_tof = local_longest_tof;

      if (local_shortest_tof < shortest_tof)
        shortest_tof = local_shortest_tof;
    }

    // PARALLEL_END_INTERUPT_REGION
  }

  /*
   * Filter events on one detector sequentially with detailed information output
   * Use the most straightforward method
   */
  void FilterEventsHighFrequency::filterSingleDetectorSequential(size_t wkspindex)
  {

    g_log.information() << "Starting of filterSingleDetectorSequential" << std::endl;

    // For each spectrum
    // a. Offset
    double percentageoffsettof = mCalibOffsets[wkspindex];

    // b. Get all events
    DataObjects::EventList events = eventWS->getEventList(wkspindex);
    std::vector<int64_t>::iterator abstimeit;
    std::vector<DataObjects::TofEvent> newevents;

    // c. Filter the event
    std::ofstream ofs;
    std::string dir = this->getProperty("OutputDirectory");
    std::string filename;
    if (dir[dir.size()-1]=='/')
      filename = dir+"eventsfilterlist.txt";
    else
      filename = dir+"/eventsfilterlist.txt";

    g_log.information() << "Output event list file = " << filename << std::endl
        << "Workspace " << wkspindex << ":  Total " << events.getNumberEvents() << " events" << std::endl;

    ofs.open(filename.c_str(), std::ios::out);
    ofs << "Pulse Time (nano-sec)" << "\t" << "Time-of-flight (ms)" << "\t"
        << "Corrected TOF" << "\t" << "Section" << std::endl;

    size_t numeventsin = 0;
    size_t numeventsout = 0;
    size_t numoutrange = 0;
    size_t numoutvalue = 0;
    for (size_t iv=0; iv<events.getNumberEvents(); iv++){
      // FOR each event

      DataObjects::TofEvent rawevent = events.getEvent(iv);

      // i.  Check negative TOF, and update loop variables
      if (rawevent.tof() < 0)
      {
        numnegtofs += 1;
        g_log.error() << "Event " << iv << " has negative TOF " << rawevent.tof() << std::endl;
        numeventsout ++;
        continue;
      }

      // ii.  Get raw event & time: Total time = pulse time (ns) + TOF*offset - sensor-sample-offset
      int64_t mtime = rawevent.m_pulsetime.total_nanoseconds()+
          static_cast<int64_t>(rawevent.m_tof*1000*percentageoffsettof)-
          mSensorSampleOffset;
      double correctedtof = rawevent.m_tof*percentageoffsettof;

      // iii. Filter out if time falls out of (T0, Tf), and update loop variables
      if (mtime < mFilterT0.total_nanoseconds() || mtime > mFilterTf.total_nanoseconds())
      {
        numeventsout ++;
        numoutrange ++;
        continue;
      }

      // iv.  Search... need to consider more situation as outside of boundary, on the grid and etc
      abstimeit = std::lower_bound(mSETimes.begin(), mSETimes.end(), mtime);
      size_t mindex;
      if (*abstimeit == mtime)
      {
        // (1) On the grid
        mindex = size_t(abstimeit-mSETimes.begin());
      }
      else if (abstimeit == mSETimes.begin())
      {
        // (2) On first grid or out of lower bound
        mindex = size_t(abstimeit-mSETimes.begin());
      }
      else
      {
        mindex = size_t(abstimeit-mSETimes.begin())-1;
      }

      // v. Filter in/out? by VALUE
      double msevalue = mSEValues[mindex];
      bool selected = false;
      if (msevalue >= mLowerLimit && msevalue <= mUpperLimit){
        DataObjects::TofEvent newevent(rawevent.m_tof, rawevent.m_pulsetime);
        newevents.push_back(newevent);
        selected = true;
        numeventsin ++;
      } else
      {
        numeventsout ++;
        numoutvalue ++;
      }

      // vi. Determine section the event belonged to
      int section;
      if (mindex == mSETimes.size()-1)
      {
        section = 0;
      } else
      {
        int64_t window = mSETimes[mindex+1]-mSETimes[mindex];
        int64_t deltime = mtime-mSETimes[mindex];
        section = static_cast<int>( static_cast<double>(deltime)/(static_cast<double>(window)/static_cast<double>(mFilterIntervals)) );
      }

      if (selected && static_cast<int>(iv) <= numOutputEvents)
      {
        ofs << iv << "\t" << rawevent.pulseTime().total_nanoseconds() << "\t" << rawevent.tof() << "\t"
            << correctedtof << "\t" << mtime << "\t" << section << std::endl;
        std::cout << iv << "\t" << rawevent.pulseTime().total_nanoseconds() << "\t" << rawevent.tof() << "\t"
            << correctedtof << "\t-->\t" << mtime << "\t" << section << std::endl;
      }
      /*
      else
      {
        std::cout << rawevent.pulseTime().total_nanoseconds() << "\t" << rawevent.tof() << "\t"
            << correctedtof << "\t" << "Not Selected" << std::endl;
      }
      */
    } // ENDFOR iv: each event

    ofs.close();

    // 3. Add to outputWS
    DataObjects::EventList* neweventlist;
    neweventlist = outputWS->getEventListPtr(wkspindex);

    double local_longest_tof = 0;
    double local_shortest_tof = 1.0E10;

    for (size_t iv=0; iv<newevents.size(); iv++){
      neweventlist->addEventQuickly(newevents[iv]);
      if (newevents[iv].m_tof > local_longest_tof)
      {
        local_longest_tof = newevents[iv].m_tof;
      }
      else if (newevents[iv].m_tof < local_shortest_tof)
      {
        local_shortest_tof = newevents[iv].m_tof;
      }
    } // ENDFOR iv

    if (local_longest_tof > longest_tof)
      longest_tof = local_longest_tof;

    if (local_shortest_tof < shortest_tof)
      shortest_tof = local_shortest_tof;

    g_log.information() << "Number of Events Selected = " << numeventsin << ",  Number of Events Not Selected = " << numeventsout << std::endl;
    g_log.information() << "NUmber of Events Outside Time Range = " << numoutrange <<
        ", Number of Events Not Within Value = " << numoutvalue << std::endl;
    g_log.information() << "Filter:  T0 = " << mFilterT0 << ", Tf = " << mFilterTf << std::endl;
    g_log.information() << "Log:     T0 = " << mSETimes[0] << "  To Filter T0 " << mSETimes[0]-mFilterT0.total_nanoseconds() << std::endl;
    g_log.information() << "Log:     Tf = " << mSETimes[mSETimes.size()-1] << "  To Filter T0 " <<
        mSETimes[mSETimes.size()-1]-mFilterT0.total_nanoseconds() << std::endl;
    g_log.information() << "Neutron 0   :   Pulse Time = " << events.getEvent(0).m_pulsetime << std::endl;
    g_log.information() << "Neutron Last:   Pulse Time = " << events.getEvent(events.getNumberEvents()-1).m_pulsetime << std::endl;
    // PARALLEL_END_INTERUPT_REGION
  }

} // namespace Mantid
} // namespace Algorithms

























