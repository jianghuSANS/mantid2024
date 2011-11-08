/*WIKI*
TODO: Enter a full wiki-markup description of your algorithm here. You can then use the Build/wiki_maker.py script to generate your full wiki page.
*WIKI*/

#include "MantidMDAlgorithms/BooleanBinaryOperationMD.h"
#include "MantidKernel/System.h"

using namespace Mantid::Kernel;
using namespace Mantid::API;

namespace Mantid
{
namespace MDAlgorithms
{

  //----------------------------------------------------------------------------------------------
  /** Constructor
   */
  BooleanBinaryOperationMD::BooleanBinaryOperationMD()
  {  }
    
  //----------------------------------------------------------------------------------------------
  /** Destructor
   */
  BooleanBinaryOperationMD::~BooleanBinaryOperationMD()
  {  }
  
  //----------------------------------------------------------------------------------------------
  /// Algorithm's name for identification. @see Algorithm::name
  const std::string BooleanBinaryOperationMD::name() const { return "BooleanBinaryOperationMD";};
  
  /// Algorithm's version for identification. @see Algorithm::version
  int BooleanBinaryOperationMD::version() const { return 1;};
  
  //----------------------------------------------------------------------------------------------
  /// Sets documentation strings for this algorithm
  void BooleanBinaryOperationMD::initDocs()
  {
    std::string algo = this->name();
    algo = algo.substr(0, algo.size()-2);
    this->setWikiSummary("Perform the " + algo + " boolean operation on two MDHistoWorkspaces");
    this->setOptionalMessage("Perform the " + algo + " boolean operation on two MDHistoWorkspaces");
  }

  //----------------------------------------------------------------------------------------------
  /// Is the operation commutative?
  bool BooleanBinaryOperationMD::commutative() const
  { return true; }

  //----------------------------------------------------------------------------------------------
  /// Check the inputs and throw if the algorithm cannot be run
  void BooleanBinaryOperationMD::checkInputs()
  {
    if (m_lhs_event || m_rhs_event)
      throw std::runtime_error("Cannot perform the " + this->name() + " operation on a MDEventWorkspace.");
    if (!acceptScalar() && (m_lhs_scalar || m_rhs_scalar))
      throw std::runtime_error("Cannot perform the " + this->name() + " operation on a WorkspaceSingleValue.");
    if (!this->commutative() && m_lhs_scalar)
      throw std::runtime_error("Cannot perform the " + this->name() + " operation with a scalar on the left-hand side.");
  }

  //----------------------------------------------------------------------------------------------
  /// Run the algorithm with an MDEventWorkspace as output
  void BooleanBinaryOperationMD::execEvent()
  {
    throw std::runtime_error("Cannot perform the " + this->name() + " operation on a MDEventWorkspace.");
  }

  //----------------------------------------------------------------------------------------------
  /// Run the algorithm with a MDHisotWorkspace as output, scalar and operand
  void BooleanBinaryOperationMD::execHistoScalar(Mantid::MDEvents::MDHistoWorkspace_sptr /*out*/, Mantid::DataObjects::WorkspaceSingleValue_const_sptr /*scalar*/)
  {
    throw std::runtime_error("Cannot perform the " + this->name() + " operation on a WorkspaceSingleValue.");
  }


} // namespace Mantid
} // namespace MDAlgorithms
