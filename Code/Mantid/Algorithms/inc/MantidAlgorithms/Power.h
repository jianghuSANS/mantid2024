

#ifndef MANTID_ALGORITHM_POWER_H_
#define MANTID_ALGORITHM_POWER_H_


//----------------------------------------------------------------------
// Includes
//----------------------------------------------------------------------
#include "MantidAlgorithms/BinaryOperation.h"

namespace Mantid
{
  namespace Algorithms
  {

  /**
      Provides the ability to raise the values in the workspace to a specified power.

      Required Properties:
      <UL>
      <LI> InputWorkspace  - The name of the workspace to correct</LI>
      <LI> OutputWorkspace - The name of the corrected workspace (can be the same as the input one)</LI>
      <LI> exponent        - The exponent to use in the power calculation</LI>
      </UL>

      @author Owen Arnold, Tessella plc
      @date 12/04/2010

      Copyright &copy; 2010 STFC Rutherford Appleton Laboratory

      This file is part of Mantid.

      Mantid is free software; you can redistribute it and/or modify
      it under the terms of the GNU General Public License as published by
      the Free Software Foundation; either version 3 of the License, or
      (at your option) any later version.

      Mantid is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty of
      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
      GNU General Public License for more details.

      You should have received a copy of the GNU General Public License
      along with this program.  If not, see <http://www.gnu.org/licenses/>.

      File change history is stored at: <https://svn.mantidproject.org/mantid/trunk/Code/Mantid>
      */

   class DLLExport Power : public BinaryOperation
   {
   public:
        /// Default constructor
        Power() : BinaryOperation() {};
        /// Destructor
         virtual ~Power() {};
        /// Algorithm's name for identification overriding a virtual method
        virtual const std::string name() const { return "Power";}
        /// Algorithm's version for identification overriding a virtual method
        virtual const int version() const { return 1;}

      private:
        // Overridden BinaryOperation methods
        void performBinaryOperation(const MantidVec& lhsX, const MantidVec& lhsY, const MantidVec& lhsE,
                                    const MantidVec& rhsY, const MantidVec& rhsE, MantidVec& YOut, MantidVec& EOut);
        void performBinaryOperation(const MantidVec& lhsX, const MantidVec& lhsY, const MantidVec& lhsE,
                                    const double& rhsY, const double& rhsE, MantidVec& YOut, MantidVec& EOut);
        void setOutputUnits(const API::MatrixWorkspace_const_sptr lhs,const API::MatrixWorkspace_const_sptr rhs,API::MatrixWorkspace_sptr out);

        //Helper method to peform the power calculation
        inline double CalculatePower(double base, double exponent);

        //Check that unsigned exponents are not being used
        inline void CheckExponent(double exponent);
   };

  }
}

#endif
