// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2022 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#include "DataManipulation.h"

#include "MantidAPI/AlgorithmManager.h"
#include "MantidAPI/MatrixWorkspace.h"
#include "MantidGeometry/Instrument.h"
#include "MantidKernel/Logger.h"
#include "MantidKernel/OptionalBool.h"

using namespace Mantid::API;
using namespace Mantid::Geometry;
using namespace Mantid::Kernel;
using Mantid::Types::Core::DateAndTime;

namespace {
Mantid::Kernel::Logger g_log("DataManipulation");
}

namespace MantidQt::CustomInterfaces {

DataManipulation::DataManipulation(QObject *parent) : InelasticTab(parent), m_tabRunning(false) {
  connect(m_batchAlgoRunner, SIGNAL(batchComplete(bool)), this, SLOT(tabExecutionComplete(bool)));
}

DataManipulation::~DataManipulation() = default;

void DataManipulation::setOutputPlotOptionsPresenter(std::unique_ptr<OutputPlotOptionsPresenter> presenter) {
  m_plotOptionsPresenter = std::move(presenter);
}

void DataManipulation::clearOutputPlotOptionsWorkspaces() { m_plotOptionsPresenter->clearWorkspaces(); }

void DataManipulation::setOutputPlotOptionsWorkspaces(std::vector<std::string> const &outputWorkspaces) {
  m_plotOptionsPresenter->setWorkspaces(outputWorkspaces);
}

void DataManipulation::runTab() {
  if (validate()) {
    m_tabStartTime = DateAndTime::getCurrentTime();
    m_tabRunning = true;
    emit updateRunButton(false, "disable", "Running...", "Running data reduction...");
    try {
      if (m_plotOptionsPresenter) {
        m_plotOptionsPresenter->clearWorkspaces();
      }
      run();
    } catch (std::exception const &ex) {
      m_tabRunning = false;
      emit updateRunButton(true, "enable");
      emit showMessageBox(ex.what());
    }
  } else {
    g_log.warning("Failed to validate input!");
  }
}

/**
 * Slot used to update the run button when an algorithm that was strted by the
 * Run button complete.
 *
 * @param error Unused
 */
void DataManipulation::tabExecutionComplete(bool error) {
  UNUSED_ARG(error);
  if (m_tabRunning) {
    m_tabRunning = false;
    runComplete(error);
    auto const enableOutputButtons = error == false ? "enable" : "disable";
    emit updateRunButton(true, enableOutputButtons);
  }
}

/**
 * Prevents the loading of data with incorrect naming if passed true
 *
 * @param filter :: true if you want to allow filtering
 */
void DataManipulation::filterInputData(bool filter) { setFileExtensionsByName(filter); }

} // namespace MantidQt::CustomInterfaces
