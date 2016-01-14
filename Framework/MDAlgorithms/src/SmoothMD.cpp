#include "MantidMDAlgorithms/SmoothMD.h"
#include "MantidAPI/FrameworkManager.h"
#include "MantidAPI/IMDHistoWorkspace.h"
#include "MantidAPI/IMDIterator.h"
#include "MantidKernel/ArrayProperty.h"
#include "MantidKernel/ArrayBoundedValidator.h"
#include "MantidKernel/CompositeValidator.h"
#include "MantidKernel/ListValidator.h"
#include "MantidKernel/MandatoryValidator.h"
#include "MantidDataObjects/MDHistoWorkspaceIterator.h"

using namespace Mantid::Kernel;
using namespace Mantid::API;
using namespace Mantid::DataObjects;

// Typedef for width vector
typedef std::vector<int> WidthVector;

// Typedef for kernel vector
typedef std::vector<double> KernelVector;

// Typedef for an optional md histo workspace
typedef boost::optional<IMDHistoWorkspace_const_sptr>
    OptionalIMDHistoWorkspace_const_sptr;

// Typedef for a smoothing function
typedef boost::function<KernelVector(const WidthVector &)> SmoothFunction;

// Typedef for a smoothing function map keyed by name.
typedef std::map<std::string, SmoothFunction> SmoothFunctionMap;

namespace {

/**
 * @brief functions
 * @return Allowed smoothing functions
 */
std::vector<std::string> functions() {
  std::vector<std::string> propOptions;
  propOptions.push_back("Hat");
  propOptions.push_back("Gaussian");
  return propOptions;
}

/**
 * Maps a function name to a function to create the corresponding kernel
 * @return function map
 */
SmoothFunctionMap makeFunctionMap(Mantid::MDAlgorithms::SmoothMD *instance) {
  SmoothFunctionMap map;
  map.insert(std::make_pair(
      "Hat",
      boost::bind(&Mantid::MDAlgorithms::SmoothMD::hatKernel, instance, _1)));
  map.insert(std::make_pair(
      "Gaussian", boost::bind(&Mantid::MDAlgorithms::SmoothMD::gaussianKernel,
                              instance, _1)));
  return map;
}
}

namespace Mantid {
namespace MDAlgorithms {

// Register the algorithm into the AlgorithmFactory
DECLARE_ALGORITHM(SmoothMD)

//----------------------------------------------------------------------------------------------
/** Constructor
 */
SmoothMD::SmoothMD() {}

//----------------------------------------------------------------------------------------------
/** Destructor
 */
SmoothMD::~SmoothMD() {}

//----------------------------------------------------------------------------------------------

/// Algorithms name for identification. @see Algorithm::name
const std::string SmoothMD::name() const { return "SmoothMD"; }

/// Algorithm's version for identification. @see Algorithm::version
int SmoothMD::version() const { return 1; }

/// Algorithm's category for identification. @see Algorithm::category
const std::string SmoothMD::category() const {
  return "MDAlgorithms\\Transforms";
}

/// Algorithm's summary for use in the GUI and help. @see Algorithm::summary
const std::string SmoothMD::summary() const {
  return "Smooth an MDHistoWorkspace according to a weight function";
}

/**
 * Smoothing performed with given kernel
 * @param toSmooth : Workspace to smooth
 * @param widthVector : Width vector
 * @param weightingWS : Weighting workspace (optional)
 * @param kernel : Kernel with which to perform smoothing
 * @return Smoothed MDHistoWorkspace
 */
IMDHistoWorkspace_sptr
SmoothMD::doSmooth(IMDHistoWorkspace_const_sptr toSmooth,
                   const WidthVector &widthVector,
                   OptionalIMDHistoWorkspace_const_sptr weightingWS,
                   const KernelVector &kernel) {

  const bool useWeights = weightingWS.is_initialized();
  uint64_t nPoints = toSmooth->getNPoints();
  Progress progress(this, 0, 1, size_t(double(nPoints) * 1.1));
  // Create the output workspace.
  IMDHistoWorkspace_sptr outWS(toSmooth->clone().release());
  progress.reportIncrement(
      size_t(double(nPoints) * 0.1)); // Report ~10% progress

  const int nThreads = Mantid::API::FrameworkManager::Instance()
                           .getNumOMPThreads(); // NThreads to Request

  auto iterators = toSmooth->createIterators(nThreads, NULL);

  PARALLEL_FOR_NO_WSP_CHECK()
  for (int it = 0; it < int(iterators.size()); ++it) {

    PARALLEL_START_INTERUPT_REGION
    boost::scoped_ptr<MDHistoWorkspaceIterator> iterator(
        dynamic_cast<MDHistoWorkspaceIterator *>(iterators[it]));

    if (!iterator) {
      throw std::logic_error(
          "Failed to cast IMDIterator to MDHistoWorkspaceIterator");
    }

    do {
      // Gets all vertex-touching neighbours
      size_t iteratorIndex = iterator->getLinearIndex();

      if (useWeights) {

        // Check that we could measure here.
        if ((*weightingWS)->getSignalAt(iteratorIndex) == 0) {

          outWS->setSignalAt(iteratorIndex,
                             std::numeric_limits<double>::quiet_NaN());

          outWS->setErrorSquaredAt(iteratorIndex,
                                   std::numeric_limits<double>::quiet_NaN());

          continue; // Skip we couldn't measure here.
        }
      }

      std::vector<size_t> neighbourIndexes =
          iterator->findNeighbourIndexesByWidth(widthVector);

      size_t nNeighbours = neighbourIndexes.size();
      double sumSignal = 0;
      double sumSqError = 0;
      for (size_t i = 0; i < neighbourIndexes.size(); ++i) {
        if (useWeights) {
          if ((*weightingWS)->getSignalAt(neighbourIndexes[i]) == 0) {
            // Nothing measured here. We cannot use that neighbouring point.
            nNeighbours -= 1;
            continue;
          }
        }
        sumSignal += toSmooth->getSignalAt(neighbourIndexes[i]);
        double error = toSmooth->getErrorAt(neighbourIndexes[i]);
        sumSqError += (error * error);
      }

      // Calculate the mean
      outWS->setSignalAt(iteratorIndex, sumSignal / double(nNeighbours));
      // Calculate the sample variance
      outWS->setErrorSquaredAt(iteratorIndex,
                               sumSqError / double(nNeighbours));

      progress.report();

    } while (iterator->next());
    PARALLEL_END_INTERUPT_REGION
  }
  PARALLEL_CHECK_INTERUPT_REGION

  return outWS;
}

//----------------------------------------------------------------------------------------------
/** Initialize the algorithm's properties.
 */
void SmoothMD::init() {
  declareProperty(new WorkspaceProperty<API::IMDHistoWorkspace>(
                      "InputWorkspace", "", Direction::Input),
                  "An input MDHistoWorkspace to smooth.");

  auto widthVectorValidator = boost::make_shared<CompositeValidator>();
  auto boundedValidator =
      boost::make_shared<ArrayBoundedValidator<int>>(1, 100);
  widthVectorValidator->add(boundedValidator);
  widthVectorValidator->add(
      boost::make_shared<MandatoryValidator<std::vector<int>>>());

  declareProperty(new ArrayProperty<int>("WidthVector", widthVectorValidator,
                                         Direction::Input),
                  "Width vector. Either specify the width in n-pixels for each "
                  "dimension, or provide a single entry (n-pixels) for all "
                  "dimensions.");

  const auto allFunctionTypes = functions();
  const std::string first = allFunctionTypes.front();

  std::stringstream docBuffer;
  docBuffer << "Smoothing function. Defaults to " << first;
  declareProperty(
      new PropertyWithValue<std::string>(
          "Function", first,
          boost::make_shared<ListValidator<std::string>>(allFunctionTypes),
          Direction::Input),
      docBuffer.str());

  declareProperty(new WorkspaceProperty<API::IMDHistoWorkspace>(
                      "InputNormalizationWorkspace", "", Direction::Input,
                      PropertyMode::Optional),
                  "Multidimensional weighting workspace. Optional.");

  declareProperty(new WorkspaceProperty<API::IMDHistoWorkspace>(
                      "OutputWorkspace", "", Direction::Output),
                  "An output smoothed MDHistoWorkspace.");
}

//----------------------------------------------------------------------------------------------
/** Execute the algorithm.
 */
void SmoothMD::exec() {

  // Get the input workspace to smooth
  IMDHistoWorkspace_sptr toSmooth = this->getProperty("InputWorkspace");

  // Get the input weighting workspace
  IMDHistoWorkspace_sptr weightingWS =
      this->getProperty("InputNormalizationWorkspace");
  OptionalIMDHistoWorkspace_const_sptr optionalWeightingWS;
  if (weightingWS) {
    optionalWeightingWS = weightingWS;
  }

  // Get the width vector
  std::vector<int> widthVector = this->getProperty("WidthVector");
  if (widthVector.size() == 1) {
    // Pad the width vector out to the right size if only one entry has been
    // provided.
    widthVector = std::vector<int>(toSmooth->getNumDims(), widthVector.front());
  }

  // Find the function to generate the chosen kernel
  const std::string smoothFunctionName = this->getProperty("Function");
  SmoothFunctionMap functionMap = makeFunctionMap(this);
  SmoothFunction smoothFunction = functionMap[smoothFunctionName];

  auto smoothing_kernel = smoothFunction(widthVector);

  // Actually perform the smoothing (convolve kernel with signal array)
  auto smoothed =
      doSmooth(toSmooth, widthVector, optionalWeightingWS, smoothing_kernel);

  setProperty("OutputWorkspace", smoothed);
}

/**
 * validateInputs
 * @return map of property names to errors.
 */
std::map<std::string, std::string> SmoothMD::validateInputs() {

  std::map<std::string, std::string> product;

  IMDHistoWorkspace_sptr toSmoothWs = this->getProperty("InputWorkspace");

  // Check the width vector
  const std::string widthVectorPropertyName = "WidthVector";
  std::vector<int> widthVector = this->getProperty(widthVectorPropertyName);

  if (widthVector.size() != 1 &&
      widthVector.size() != toSmoothWs->getNumDims()) {
    product.insert(std::make_pair(widthVectorPropertyName,
                                  widthVectorPropertyName +
                                      " can either have one entry or needs to "
                                      "have entries for each dimension of the "
                                      "InputWorkspace."));
  } else {
    for (auto it = widthVector.begin(); it != widthVector.end(); ++it) {
      const int widthEntry = *it;
      if (widthEntry % 2 == 0) {
        std::stringstream message;
        message << widthVectorPropertyName
                << " entries must be odd numbers. Bad entry is " << widthEntry;
        product.insert(std::make_pair(widthVectorPropertyName, message.str()));
      }
    }
  }

  // Check the dimensionality of the normalization workspace
  const std::string normalisationWorkspacePropertyName =
      "InputNormalizationWorkspace";

  IMDHistoWorkspace_sptr normWs =
      this->getProperty(normalisationWorkspacePropertyName);
  if (normWs) {
    const size_t nDimsNorm = normWs->getNumDims();
    const size_t nDimsSmooth = toSmoothWs->getNumDims();
    if (nDimsNorm != nDimsSmooth) {
      std::stringstream message;
      message << normalisationWorkspacePropertyName
              << " has a different number of dimensions than InputWorkspace. "
                 "Shapes of inputs must be the same. Cannot continue "
                 "smoothing.";
      product.insert(
          std::make_pair(normalisationWorkspacePropertyName, message.str()));
    } else {
      // Loop over dimensions and check nbins.
      for (size_t i = 0; i < nDimsNorm; ++i) {
        const size_t nBinsNorm = normWs->getDimension(i)->getNBins();
        const size_t nBinsSmooth = toSmoothWs->getDimension(i)->getNBins();
        if (nBinsNorm != nBinsSmooth) {
          std::stringstream message;
          message << normalisationWorkspacePropertyName
                  << ". Number of bins from dimension with index " << i
                  << " do not match. " << nBinsSmooth << " expected. Got "
                  << nBinsNorm << ". Shapes of inputs must be the same. Cannot "
                                  "continue smoothing.";
          product.insert(std::make_pair(normalisationWorkspacePropertyName,
                                        message.str()));
          break;
        }
      }
    }
  }

  return product;
}

/*
 * Create a Gaussian kernel. The returned kernel is a 1D vector,
 * the order of which matches the linear indices returned by
 * the findNeighbourIndexesByWidth method.
 * @param widths : Width vector
 * @return The Gaussian kernel
 */
KernelVector SmoothMD::gaussianKernel(const WidthVector &widths) {
  // TODO implement
  KernelVector kernel;
  return kernel;
}

/*
 * Create a top hat kernel. The returned kernel is a 1D vector,
 * the order of which doesn't matter because all elements are
 * the same.
 * @param widths : Width vector
 * @return The hat kernel
 */
KernelVector SmoothMD::hatKernel(const WidthVector &widths) {
  size_t kernel_length = 1;
  for (auto width : widths) {
    kernel_length *= width;
  }

  // Explicitly cast the length to avoid compiler warnings
  double hat_amplitude_normalised = 1.0 / static_cast<double>(kernel_length);
  KernelVector kernel(kernel_length, hat_amplitude_normalised);
  return kernel;
}

} // namespace MDAlgorithms
} // namespace Mantid
