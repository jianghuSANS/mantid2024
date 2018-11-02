// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
//     NScD Oak Ridge National Laboratory, European Spallation Source
//     & Institut Laue - Langevin
// SPDX - License - Identifier: GPL - 3.0 +
#ifndef MANTID_GPUALGORITHMS_GPUALGORITHMTEST_H_
#define MANTID_GPUALGORITHMS_GPUALGORITHMTEST_H_

#include "MantidKernel/System.h"
#include "MantidKernel/Timer.h"
#include <cxxtest/TestSuite.h>
#include <iomanip>

#include "MantidGPUAlgorithms/GPUAlgorithm.h"

using namespace Mantid;
using namespace Mantid::GPUAlgorithms;
using namespace Mantid::API;

class GPUAlgorithmTest : public CxxTest::TestSuite {
public:
  void test_1() {}
};

#endif /* MANTID_GPUALGORITHMS_GPUALGORITHMTEST_H_ */
