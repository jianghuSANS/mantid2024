/*WIKI*

* A list of vanadium peak positions in d-spacing is used for the central peak positions: 0.5044,0.5191,0.5350,0.5526,0.5936,0.6178,0.6453,0.6768,0.7134,0.7566,0.8089,0.8737,0.9571,1.0701,1.2356,1.5133,2.1401

* StripPeaks is called by providing the list of vanadium peak positions.

* The vanadium peaks are fit to a function combined from Gaussian and linear/quadratic background.

*WIKI*/

#include "MantidAlgorithms/StripVanadiumPeaks2.h"
#include "MantidKernel/System.h"

using namespace Mantid::Kernel;
using namespace Mantid::API;

namespace Mantid
{
namespace Algorithms
{

DECLARE_ALGORITHM(StripVanadiumPeaks2)

  //----------------------------------------------------------------------------------------------
  /** Constructor
   */
  StripVanadiumPeaks2::StripVanadiumPeaks2()
  {
  }
    
  //----------------------------------------------------------------------------------------------
  /** Destructor
   */
  StripVanadiumPeaks2::~StripVanadiumPeaks2()
  {
  }

  void StripVanadiumPeaks2::initDocs()
  {
    this->setWikiSummary("This algorithm removes peaks (at vanadium d-spacing positions by default) out of a background by linearly/quadratically interpolating over the expected peak positions. ");
  }
  
  void StripVanadiumPeaks2::init()
  {
    // Declare inputs and output.  Copied from StripPeaks

    declareProperty(
      new WorkspaceProperty<>("InputWorkspace","",Direction::Input),
      "Name of the input workspace. If you use the default vanadium peak positions are used, the workspace must be in units of d-spacing." );

    declareProperty(new WorkspaceProperty<>("OutputWorkspace","",Direction::Output),
      "The name of the workspace to be created as the output of the algorithm.\n"
      "If the input workspace is an EventWorkspace, then the output must be different (and will be made into a Workspace2D)." );

    BoundedValidator<int> *min = new BoundedValidator<int>();
    min->setLower(1);
    // The estimated width of a peak in terms of number of channels
    declareProperty("FWHM", 7, min,
      "The number of points covered, on average, by the fwhm of a peak. Passed through to FindPeaks. Default 7." );

    // The tolerance allowed in meeting the conditions
    declareProperty("Tolerance",4, min->clone(),
      "A measure of the strictness desired in meeting the condition on peak candidates. Passed through to FindPeaks. Default 4.");

    std::vector<std::string> bkgdtypes;
    bkgdtypes.push_back("Linear");
    bkgdtypes.push_back("Quadratic");
    declareProperty("BackgroundType", "Linear", new ListValidator(bkgdtypes),
        "The type of background of the histogram. Present choices include Linear and Quadratic. ");

    declareProperty("HighBackground", true,
        "Flag to indicate that the peaks are relatively weak comparing to background ");

    declareProperty("PeakPositionTolerance", -1.0,
        "Tolerance on the found peaks' positions against the input peak positions. A non-positive value turns this option off. ");

    BoundedValidator<int> *mustBePositive = new BoundedValidator<int>();
    mustBePositive->setLower(0);
    declareProperty("WorkspaceIndex",EMPTY_INT(),mustBePositive,
      "If set, peaks will only be removed from this workspace index (otherwise from all) ");

    return;

  }

  void StripVanadiumPeaks2::exec(){

    // 1. Process input/output
    API::MatrixWorkspace_sptr inputWS = getProperty("InputWorkspace");
    std::string outputWSName = getPropertyValue("OutputWorkspace");
    int singleIndex = getProperty("WorkspaceIndex");
    int param_fwhm = getProperty("FWHM");
    int param_tolerance = getProperty("Tolerance");

    bool singleSpectrum = !isEmpty(singleIndex);

    // 2. Call StripPeaks
    std::string peakpositions;
    std::string unit = inputWS->getAxis(0)->unit()->unitID();
    if (unit == "dSpacing"){
      peakpositions = "0.5044,0.5191,0.5350,0.5526,0.5936,0.6178,0.6453,0.6768,0.7134,0.7566,0.8089,0.8737,0.9571,1.0701,1.2356,1.5133,2.1401";

    } else if (unit == "MomentumTransfer"){
      g_log.error() << "Unit MomentumTransfer (Q-space) is NOT supported by StripVanadiumPeaks now.\n";
      throw std::invalid_argument("Q-space is not supported");
      // Comment out next line as it won't be reached.
      //peakpositions = "2.9359, 4.1520, 5.0851, 5.8716, 6.5648, 7.1915, 7.7676, 8.3045, 8.8074, 9.2837, 9.7368, 10.1703, 10.5849, 11.3702, 11.7443, 12.1040, 12.4568";

    } else {
      g_log.error() << "Unit " << unit << " Is NOT supported by StripVanadiumPeaks, which only supports d-spacing" << std::endl;
      throw std::invalid_argument("Not supported unit");
    }

    // Call StripPeak
    double pro0 = 0.0;
    double prof = 1.0;
    bool sublog = true;
    IAlgorithm_sptr stripPeaks = createSubAlgorithm("StripPeaks", pro0, prof, sublog);
    stripPeaks->setProperty("InputWorkspace", inputWS);
    stripPeaks->setPropertyValue("OutputWorkspace", outputWSName);
    stripPeaks->setProperty("FWHM", param_fwhm);
    stripPeaks->setProperty("Tolerance", param_tolerance);
    stripPeaks->setPropertyValue("PeakPositions", peakpositions);
    stripPeaks->setProperty<std::string>("BackgroundType", getProperty("BackgroundType"));
    stripPeaks->setProperty<bool>("HighBackground", getProperty("HighBackground"));
    if (singleSpectrum){
      stripPeaks->setProperty("WorkspaceIndex", singleIndex);
    }
    stripPeaks->setProperty<double>("PeakPositionTolerance", getProperty("PeakPositionTolerance"));

    stripPeaks->executeAsSubAlg();

    // 3. Get and set output workspace
    // API::MatrixWorkspace_sptr outputWS = boost::dynamic_pointer_cast<API::MatrixWorkspace_sptr>(AnalysisDataService::Instance().retrieve(outputWSName));
    // boost::shared_ptr<API::Workspace> outputWS = AnalysisDataService::Instance().retrieve(outputWSName);
    API::MatrixWorkspace_sptr outputWS = stripPeaks->getProperty("OutputWorkspace");

    this->setProperty("OutputWorkspace", outputWS);

    return;
  }



} // namespace Mantid
} // namespace Algorithms

