// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2007 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
//
// PythonStdoutChannel.h
//
// Similar to console channel for logging. The output is on std::cout instead of
// std::clog (which is the same as std::cerr)
// Usage: use in it Mantid.properties or mantid.user.properties instead of
// ConsoleChannel class
//
//
//

#pragma once

// local includes
#include "MantidPythonInterface/core/DllConfig.h"
#include "MantidPythonInterface/core/GlobalInterpreterLock.h"
#include "MantidPythonInterface/core/WrapPython.h"

// 3rd-party includes
#include "MantidKernel/StdoutChannel.h"
#include <Poco/ConsoleChannel.h>
//#include <pybind11/iostream.h>

// standard includes
#include <iostream>

namespace Poco {

class MANTID_PYTHONINTERFACE_CORE_DLL PythonStdoutChannel : public ConsoleChannel {
public:
  /// Constructor for PythonStdoutChannel
  PythonStdoutChannel();
};

/* /// Store in case class PythonStdoutChannel is not up to the task and needs to be replaced with this class
class MANTID_PYTHONINTERFACE_CORE_DLL PyBindStdoutChannel : public StdoutChannel {

public:
  /// Constructor for PythonStdoutChannel
  PyBindStdoutChannel() : StdoutChannel() {}

private:
  pybind11::scoped_ostream_redirect m_redirect;
};
*/

} // namespace Poco
