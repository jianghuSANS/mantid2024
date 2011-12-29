/*WIKI*
 This algorithm Corrects the time of flight (TOF) of an indirect geometry instrument by a time offset that is dependent on the velocity of the neutron after passing through the moderator.
 The TOF measured by the BASIS data acquisition system (DAS) should be reduced by this moderator emission time. The DAS "erroneously"
 thinks that it takes longer for neutrons to reach the sample and detectors, because it does not "know" that the neutrons
 spend some time in the moderator before being emitted and starting flying

 A heuristic formula for the correction, stored in the instrument definition file, is taken as linear on the initial neutron wavelength lambda_i:
     t_0 = gradient * lambda_i + intercept,  [gradient]=microsec/Angstrom and [intercept]=microsec

Required Properties:
  InputWorkspace  - EventWorkSpace in TOF units. </LI>
  OutputWorkspace - EventWorkSpace in TOF units. </LI>
  Instrument Geometry - Indirect (obtained from the instrument parameter file)
  Moderator.Tzero.gradient - Variation of the time offset with initial neutron wavelength (obtained from the instrument parameter file)
  Moderator.Tzero.intercept - time offset common to all neutrons (obtained from the instrument parameter file)

  The recorded TOF = t_0 + t_i + t_f with
 	 t_0: moderator emission time
  	 t_i: time from moderator to sample
  	 t_f: time from sample to detector

This algorithm will replace TOF with TOF' = TOF-t_0 = t_i+t_f

	 For a direct geometry instrument, lambda_i is (approximately) the same for all neutrons. Hence the moderator emission time is the same for all neutrons.
	     There is already an algorithm, getEi, that calculates t_0 for the direct geometry instrument. Thus we skip this step.
 	 For an indirect geometry instrument, lambda_i is not known but the final energy, E_f, selected by the analyzers is known. For this geometry:
 	 	 t_f = L_f/v_f   L_f: distance from sample to detector, v_f: final velocity derived from E_f
 	 	 t_i = L_i/v_i   L_i: distance from moderator to sample, v_i: initial velocity unknown
 	 	 t_0 = a/v_i+b   a and b are constants derived from the aforementioned heuristic formula.
 	 	                 a=gradient*3.956E-03, [a]=meter,    b=intercept, [b]=microsec
 	 	 Putting all together:  TOF' = (L_i/(L_i+a))*(TOF-t_f-b) + t_f,   [TOF']=microsec
*WIKI*/

//----------------------------------------------------------------------
// Includes
//----------------------------------------------------------------------
#include "MantidAlgorithms/ModeratorTzero.h"
#include "MantidAPI/WorkspaceValidators.h"
#include "MantidDataObjects/Workspace2D.h"
#include "MantidDataObjects/EventWorkspace.h"
#include "MantidKernel/UnitFactory.h"

namespace Mantid
{
namespace Algorithms
{

// Register the algorithm into the AlgorithmFactory
DECLARE_ALGORITHM(ModeratorTzero)

/// Sets documentation strings for this algorithm
void ModeratorTzero::initDocs()
{
  setWikiSummary(" Corrects the time of flight of an indirect geometry instrument by a time offset that is dependent on the velocity of the neutron after passing through the moderator. ");
  setOptionalMessage(" Corrects the time of flight of an indirect geometry instrument by a time offset that is dependent on the velocity of the neutron after passing through the moderator.");
}

using namespace Mantid::Kernel;
using namespace Mantid::API;
using namespace Mantid::Geometry;
using namespace Mantid::DataObjects;

// A reference to the logger is provided by the base class, it is called g_log.
// It is used to print out information, warning and error messages

void ModeratorTzero::init()
{

  CompositeWorkspaceValidator<> *wsValidator = new CompositeWorkspaceValidator<>;
  wsValidator->add(new WorkspaceUnitValidator<>("TOF"));
  declareProperty(new WorkspaceProperty<MatrixWorkspace>("InputWorkspace","",Direction::Input,wsValidator),
		  "The name of the input workspace, containing events and/or histogram data, in units of time-of-flight");
  //declare the output workspace
  declareProperty(new WorkspaceProperty<MatrixWorkspace>("OutputWorkspace","",Direction::Output),
		  "The name of the output workspace");

} // end of void ModeratorTzero::init()

void ModeratorTzero::exec()
{
  //retrieve the input workspace.
  const MatrixWorkspace_sptr inputWS = getProperty("InputWorkspace");

  //Get a pointer to the instrument contained in the workspace
  Instrument_const_sptr instrument = inputWS->getInstrument();

  //deltaE-mode (should be "indirect")
  std::string Emode;
  try
  {
	Emode = instrument->getStringParameter("deltaE-mode")[0];
    g_log.debug() << "Instrument Geometry: " << Emode << std::endl;
    if(Emode != "indirect")
      throw std::invalid_argument("Instrument geometry must be of type indirect.");
  }
  catch (Exception::NotFoundError &)
  {
    g_log.error("Unable to retrieve instrument geometry (direct or indirect) parameter");
    throw Exception::InstrumentDefinitionError("Unable to retrieve instrument geometry (direct or indirect) parameter", inputWS->getTitle());
  }

  //gradient, intercept constants
  double gradient;
  try
  {
	gradient = instrument->getNumberParameter("Moderator.TimeZero.gradient")[0]; //[gradient]=microsecond/Angstrom
	//conversion factor for gradient from microsecond/Angstrom to meters
	double convfactor = 1e+4*PhysicalConstants::h/PhysicalConstants::NeutronMass;
	gradient *= convfactor; //[gradient] = meter
	this->intercept = instrument->getNumberParameter("Moderator.TimeZero.intercept")[0]; //[intercept]=microsecond
    g_log.debug() << "Moderator Time Zero: gradient=" << gradient << "intercept=" << this->intercept << std::endl;
  }
  catch (Exception::NotFoundError &)
  {
    g_log.error("Unable to retrieve Moderator Time Zero parameters (gradient and intercept)");
    throw Exception::InstrumentDefinitionError("Unable to retrieve Moderator Time Zero parameters (gradient and intercept)", inputWS->getTitle());
  }

  //distance L_i between source and sample ([Li]=meters). Calculate scaling
  IObjComponent_const_sptr source = instrument->getSource();
  IObjComponent_const_sptr sample = instrument->getSample();
  double L_i;
  try
  {
    L_i = source->getDistance(*sample);
    g_log.debug() << "Source-sample distance: " << L_i << std::endl;
  }
  catch (Exception::NotFoundError &)
  {
    g_log.error("Unable to calculate source-sample distance");
    throw Exception::InstrumentDefinitionError("Unable to calculate source-sample distance", inputWS->getTitle());
  }
  this->scaling = L_i/(L_i+gradient);

  //Run execEvent if eventWorkSpace
  EventWorkspace_const_sptr eventWS = boost::dynamic_pointer_cast<const EventWorkspace>(inputWS);
  if (eventWS != NULL)
  {
    execEvent();
    return;
  }

  MatrixWorkspace_sptr outputWS;
  //Check whether input = output to see whether a new workspace is required.
  if (getPropertyValue("InputWorkspace") == getPropertyValue("OutputWorkspace"))
  {
	  outputWS = inputWS;
  }else
  {
    //Create new workspace for output from old
    outputWS = WorkspaceFactory::Instance().create(inputWS);
    outputWS->isDistribution(inputWS->isDistribution());
  }

  // do the shift in X
  const int64_t numHists = static_cast<int64_t>(inputWS->getNumberHistograms());
  //Progress prog(this,0.0,1.0,numHists); //report progress of algorithm
  //PARALLEL_FOR2(inputWS, outputWS)
  for (int64_t i=0; i < numHists; ++i)
  {
	//PARALLEL_START_INTERUPT_REGION
    // Calculate the time t_f from sample to detector 'i'
    double t_f = CalculateTf(sample,inputWS,i);
	// shift the time of flights
    if(t_f > 0) //t_f < 0 when no detector info is available
    {
      double offset = (1-this->scaling)*t_f - this->scaling*this->intercept;;
      MantidVec &inbins = inputWS->dataX(i);
      MantidVec &outbins = outputWS->dataX(i);
      for(unsigned int j=0; j < inbins.size(); j++)
      {
      	outbins[j] = this->scaling*inbins[j] + offset;
      }
      g_log.debug() << "inbins[0]=" << inbins[0] << " outbins[0]=" << outbins[0] << std::endl;
      g_log.debug() << "inbins[last] " << inbins[inbins.size()-1] << " outbins[last]" << outbins[inbins.size()-1] << std::endl;
    }else
    {
      outputWS->dataX(i) = inputWS->dataX(i);
    }
    //Copy y and e data
    outputWS->dataY(i) = inputWS->dataY(i);
    outputWS->dataE(i) = inputWS->dataE(i);

	//prog.report();
	//PARALLEL_END_INTERUPT_REGION
  }
  //PARALLEL_CHECK_INTERUPT_REGION

  // Copy units
  if (inputWS->getAxis(0)->unit().get())
  outputWS->getAxis(0)->unit() = inputWS->getAxis(0)->unit();
  try
  {
    if(inputWS->getAxis(1)->unit().get())
      outputWS->getAxis(1)->unit() = inputWS->getAxis(1)->unit();
  }
  catch(Exception::IndexError &) {
      // OK, so this isn't a Workspace2D
  }

    // Assign it to the output workspace property
    setProperty("OutputWorkspace",outputWS);
}

void ModeratorTzero::execEvent(){
  g_log.information("Processing event workspace");

  const MatrixWorkspace_const_sptr matrixInputWS = getProperty("InputWorkspace");
  EventWorkspace_const_sptr inputWS= boost::dynamic_pointer_cast<const EventWorkspace>(matrixInputWS);

  // generate the output workspace pointer
  const int64_t numHists = static_cast<int64_t>(inputWS->getNumberHistograms());
  Mantid::API::MatrixWorkspace_sptr matrixOutputWS = getProperty("OutputWorkspace");
  EventWorkspace_sptr outputWS;
  if (matrixOutputWS == matrixInputWS)
    outputWS = boost::dynamic_pointer_cast<EventWorkspace>(matrixOutputWS);
  else
  {
    //Make a brand new EventWorkspace
    outputWS = boost::dynamic_pointer_cast<EventWorkspace>(WorkspaceFactory::Instance().create("EventWorkspace", numHists, 2, 1));
    //Copy geometry over.
    WorkspaceFactory::Instance().initializeFromParent(inputWS, outputWS, false);
    //You need to copy over the data as well.
    outputWS->copyDataFrom( (*inputWS) );
    //Cast to the matrixOutputWS and save it
    matrixOutputWS = boost::dynamic_pointer_cast<MatrixWorkspace>(outputWS);
    setProperty("OutputWorkspace", matrixOutputWS);
  }

  //Get a pointer to the sample
  IObjComponent_const_sptr sample = outputWS->getInstrument()->getSample();

  // Loop over the spectra
  Progress prog(this,0.0,1.0,numHists); //report progress of algorithm
  //PARALLEL_FOR1(outputWS)
  for (int64_t i = 0; i < int64_t(numHists); ++i)
  {
	//PARALLEL_START_INTERUPT_REGION
	EventList &evlist=outputWS->getEventList(i);
	if( evlist.getNumberEvents() > 0 ) //don't bother with empty lists
	{
	  // Calculate the time from sample to detector 'i'
	  double t_f = CalculateTf(sample,matrixOutputWS,i);
	  if(t_f > 0)
	  {
		//Calculate new time of flight, TOF'=scaling*(TOF-t_f-intercept)+t_f = scaling*TOF + (1-scaling)*t_f - scaling*intercept
		g_log.debug() << evlist.dataX()[0] << " " << evlist.dataX()[1] << std::endl;
		evlist.convertTof(this->scaling, (1-this->scaling)*t_f - this->scaling*intercept);
		g_log.debug() << this->scaling << " " << (1-this->scaling)*t_f - this->scaling*intercept << std::endl;
		g_log.debug() << evlist.dataX()[0] << " " << evlist.dataX()[1] << std::endl;
	  }
	}
    prog.report();
    //PARALLEL_END_INTERUPT_REGION
  }
  //PARALLEL_CHECK_INTERUPT_REGION
  outputWS->clearMRU(); // Clears the Most Recent Used lists */
} // end of void ModeratorTzero::execEvent()

  //calculate time from sample to detector. Determined only by detector specs
  double ModeratorTzero::CalculateTf(IObjComponent_const_sptr sample, MatrixWorkspace_sptr inputWS, int64_t i){
	static const double convFact = sqrt(2*PhysicalConstants::meV/PhysicalConstants::NeutronMass);
	static const double TfError = -1.0; //signal error when calculating final time
    // Get detector position
	IDetector_const_sptr det;
	try
	{
	  det = inputWS->getDetector(i);
	} catch (Exception::NotFoundError&)
	{
	  g_log.error("Detector not found");
	  return TfError;
	}

	// Get final energy E_f, final velocity v_f
	double E_f, v_f, t_f;
	std::vector< double >  wsProp=det->getNumberParameter("Efixed");
	if ( wsProp.size() > 0 )
	{
	  E_f = wsProp.at(0); //[E_f]=meV
	  v_f = convFact * sqrt(E_f);
	  g_log.debug() << "detector: " << i << " E_f:="<< E_f << " v_f=" << v_f << std::endl;
	  //obtain L_f, calculate t_f
	  double L_f;
	  try
	  {
	    L_f = det->getDistance(*sample);
	    t_f = L_f / v_f;
	    g_log.debug() << "dectector " << i << " L_f=" << L_f << " t_f=" << t_f << std::endl;
	  }
	  catch (Exception::NotFoundError &)
	  {
	    g_log.error("Unable to calculate detector-sample distance");
	    throw Exception::InstrumentDefinitionError("Unable to calculate detector-sample distance", inputWS->getTitle());
	  }
	}
	else
	{
	  g_log.debug() <<"Efixed not found for detector "<< i << std::endl;
	  return TfError;
	}
	return t_f;
  } // end of CalculateTf(const MatrixWorkspace_sptr inputWS, int64_t i)

} // namespace Algorithms
} // namespace Mantid

